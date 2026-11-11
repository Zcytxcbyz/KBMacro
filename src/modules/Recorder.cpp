#include "modules/Recorder.hpp"

#include <windows.h>

#include <wil/resource.h>

#include <algorithm>
#include <array>
// ReSharper disable once CppUnusedIncludeDirective
#include <cstddef>
#include <cstdint>
#include <moodycamel/concurrentqueue.h>
#include <optional>
#include <vector>

#include "common/Log.hpp"
#include "common/RateLimiter.hpp"
#include "modules/RecorderNormalize.hpp"
#include "win32/Input.hpp"

// ============================================================================
// Recording module: two low-level hooks capture keyboard/mouse events, normalized into an Action list.
//
// Thread model (everything except the UI calls at both ends happens on the recording thread):
//
//   UI thread                      recording thread
//   ─────────                      ────────────────
//   StartRecord(opts, cb) ────────→ run()
//                                      ├ start delay (timer.wait, interruptible)
//                                      ├ PeekMessage(PM_NOREMOVE)  create message queue
//                                      ├ SetWindowsHookEx ×2       install hooks
//                                      ├ stop_callback             wakes me on stop
//                                      └ pump loop ←── the system calls the callbacks here
//   StopRecord() ─── request_stop ─────┘
//                └── join waits for the finish
//
// Two rules that span the whole file:
//
//   [Rule 1] the hook callback is a hot path, it may only "copy params + enqueue + pass through".
//     low-level hooks sit directly on the system input chain, the system gives each callback 300ms
//     (LowLevelHooksTimeout, the registry default); on timeout it silently removes the hook and no event
//     arrives anymore. so the callback has no filtering, no sampling, no logging and no allocation.
//
//   [Rule 2] deciding, filtering and translating all happen in the pump.
//     the pump runs on its own thread, is not bound by the 300ms, and can take 256 at a time (saves atomics).
//     the price is that unwanted events must be queued first too, so capacity is worst-case sized at 65536.
// ============================================================================

namespace {

// ─────────────────────────────── constants ───────────────────────────────

// Stop signal. WM_APP and above is the application range, it never collides with system messages.
//   posted by PostThreadMessage inside stop_callback, to wake the pump blocked in the wait
//   immediately (instead of waiting for the next 1-second tick).
constexpr UINT kStopMessage = WM_APP + 0x4B;

// Queue capacity. Because the callback does not filter, even unwanted move events are queued first:
//   125Hz mouse → 125 records/s; 65536 records ≈ 8.7 minutes of continuous movement.
//   each RawHookEvent is about 48 bytes, so about 3MB preallocated; one-off, zero allocation afterwards.
constexpr std::size_t kQueueCapacity = 65536;

// How many records one bulk take pulls. Bulk take reduces the queue's atomic operations.
constexpr std::size_t kDrainBatch = 256;

// Rate-limit interval for progress logs (handed to RateLimiter, the class does not care about "how long").
constexpr auto kProgressInterval = ITimer::Duration{1000};

// Early-stop factor for the raw event limit. One Action comes from at most 2 raw events
//   (click = Down + Up; a drag adds intermediate Moves, so the worst ratio 2 is used here).
//   exact truncation is still done by normalize by "action count", this only prevents endless recording.
constexpr std::size_t kRawPerAction = 2;

// Event source. Keyboard and mouse msg ranges overlap (both are WM_xxx in the 0x0200 range),
//   so the source must be recorded separately, msg alone is not enough.
constexpr std::uint8_t kSourceKeyboard = 0;
constexpr std::uint8_t kSourceMouse = 1;

// ───────────────────────── records the callback writes to the queue ─────────────────────────

// Field-by-field copy of the hook struct, with no interpretation.
//   why not store KBDLLHOOKSTRUCT / MSLLHOOKSTRUCT directly?
//   they go into the same queue and the source must be distinguishable; dropping unused fields
//   (like scanCode) makes each record smaller, so the queue holds more.
struct RawHookEvent {
    std::uint8_t source = kSourceKeyboard;

    std::uint32_t msg = 0; // raw wParam: WM_KEYDOWN / WM_LBUTTONDOWN / WM_MOUSEWHEEL …
    std::uint32_t vk = 0; // keyboard: vkCode
    std::int32_t x = 0; // mouse: pt.x (virtual-desktop coordinates)
    std::int32_t y = 0; // mouse: pt.y
    std::uint32_t mouseData = 0; // wheel: high 16 bits are signed delta; side: high 16 bits are XBUTTON1/2
    std::uint32_t flags = 0; // LLKHF_INJECTED / LLMHF_INJECTED
    std::uintptr_t extraInfo = 0; // injection marker: our own events carry 'KBMR'
    std::uint32_t time = 0; // time from the hook struct (GetTickCount ms, wraps)
};

// The only channel between the callback and the pump.
//   thread_local: the hook must be installed on the thread owning it, and the callback is always called
//   on that same thread, so no lock, atomic variable or memory barrier is needed here.
thread_local moodycamel::ConcurrentQueue<RawHookEvent> *t_queue = nullptr;

// Count of failed enqueues. This is the only thing the callback may write, because "enqueue succeeded
//   or not" is visible only to the callback itself (the pump cannot see the dropped ones); it makes no
//   judgement about events, it just records an objective failure for the exit log.
thread_local std::size_t t_droppedQueueFull = 0;

// Clear t_queue when leaving scope: even if run() is torn down by an exception, no dangling
//   pointer to a destroyed queue is left behind for system callbacks to use.
struct QueueGuard {
    ~QueueGuard() { t_queue = nullptr; }
};

// ─────────────────── hook callbacks (hot path: copy params + enqueue only) ───────────────────

// Low-level keyboard hook. Note:
//   - the nCode check is API protocol, not an event decision (the struct is invalid when nCode < 0)
//   - CallNextHookEx must run whether or not the event is dropped, otherwise the app never sees the key
//   - time budget: 48-byte copy + one try_enqueue ≈ tens of nanoseconds
LRESULT CALLBACK onKeyboard(const int nCode, const WPARAM wParam, const LPARAM lParam) {
    if (nCode == HC_ACTION && t_queue != nullptr) {
        const auto *data = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);

        RawHookEvent e;
        e.source = kSourceKeyboard;
        e.msg = static_cast<std::uint32_t>(wParam);
        e.vk = data->vkCode;
        e.flags = data->flags;
        e.extraInfo = data->dwExtraInfo;
        e.time = data->time;

        // queue full: drop this one, no retry, no wait.
        //   a gap in the recording is better than system input stalling on the hook chain.
        if (!t_queue->try_enqueue(e)) ++t_droppedQueueFull;
    }
    return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// Low-level mouse hook, fully symmetric in structure to the keyboard one.
LRESULT CALLBACK onMouse(const int nCode, const WPARAM wParam, const LPARAM lParam) {
    if (nCode == HC_ACTION && t_queue != nullptr) {
        const auto *data = reinterpret_cast<const MSLLHOOKSTRUCT *>(lParam);

        RawHookEvent e;
        e.source = kSourceMouse;
        e.msg = static_cast<std::uint32_t>(wParam);
        e.x = data->pt.x;
        e.y = data->pt.y;
        e.mouseData = data->mouseData;
        e.flags = data->flags;
        e.extraInfo = data->dwExtraInfo;
        e.time = data->time;

        if (!t_queue->try_enqueue(e)) ++t_droppedQueueFull;
    }
    return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// ───────────────────────────── pump-side state ─────────────────────────────

// Counts of the five dropped categories. Only for the exit log -- when debugging "why nothing was
//   recorded", the raw/actions numbers alone do not show what ate the events.
struct Dropped {
    std::size_t injected = 0; // own injection ('KBMR') or injection the user chose to filter
    std::size_t packet = 0; // VK_PACKET (IME-synthesized, cannot be played back)
    std::size_t hotkey = 0; // main keys of the start/stop hotkeys
    std::size_t sampled = 0; // dropped by move sampling
};

struct CaptureState {
    const recorder::RecordOptions &opts;

    std::vector<recorder::RawEvent> raw; // filtered/translated events, later handed to normalize
    Dropped dropped;

    // Time of the last "accepted" mouse move. Advanced only on accept -- otherwise a slow drag would
    //   be eaten entirely because each step is below the sampling interval.
    std::uint32_t lastMoveTime = 0;
};

// Side buttons: the high 16 bits of mouseData are XBUTTON1 / XBUTTON2.
[[nodiscard]] MouseButton buttonOf(const RawHookEvent &e) noexcept {
    switch (e.msg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            return MouseButton::Left;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            return MouseButton::Right;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return MouseButton::Middle;
        default:
            break;
    }
    return (HIWORD(e.mouseData) == XBUTTON2) ? MouseButton::Side2 : MouseButton::Side1;
}

// Decision table: returning false means drop it, the reason is counted.
//   order is deliberate -- the first three ignore user config (hard rules), so they precede the switches.
[[nodiscard]] bool accept(CaptureState &state, const RawHookEvent &e) {
    const recorder::RecordOptions &opts = state.opts;
    const bool keyboard = (e.source == kSourceKeyboard);

    // 1) events we injected ourselves: always filtered. otherwise "record while playing" records itself.
    if (e.extraInfo == input::kInjectedSignature) {
        ++state.dropped.injected;
        return false;
    }

    // 2) VK_PACKET: keys synthesized by an IME, vk holds no real key code, they cannot be played back.
    if (keyboard && e.vk == VK_PACKET) {
        ++state.dropped.packet;
        return false;
    }

    // 3) main keys of enabled global hotkeys: consumed by the hotkey system during recording, never reaching
    //    the app; recording them would only trigger us on playback (F1 starts the clicker, F10 starts all).
    //    filter main keys only, never modifiers: with Ctrl in the stop hotkey, filtering Ctrl throughout would
    //    eat a normal Ctrl+C as "only C". normalization strips the residual runs at head and tail.
    if (keyboard && e.vk != 0 && std::ranges::find(opts.ignoredKeys, e.vk) != opts.ignoredKeys.end()) {
        ++state.dropped.hotkey;
        return false;
    }

    // 4) events injected by other programs: user's choice. off by default, because "nothing recorded" is
    //    harder to debug than "dirty data recorded". the flags differ per source, so pick by source.
    const std::uint32_t injectedFlag = keyboard ? LLKHF_INJECTED : LLMHF_INJECTED;
    if (opts.ignoreInjected && (e.flags & injectedFlag) != 0) {
        ++state.dropped.injected;
        return false;
    }

    // 5) mouse move sampling: dropped when the gap since the last "accepted" move is below the interval.
    //    time wraps, so the comparison must use unsigned differences (not a plain < on the values).
    if (!keyboard && e.msg == WM_MOUSEMOVE) {
        const auto interval = static_cast<std::uint32_t>(std::max(0, opts.moveIntervalMs));
        if (interval > 0 && (e.time - state.lastMoveTime) < interval) {
            ++state.dropped.sampled;
            return false;
        }
        state.lastMoveTime = e.time;
    }

    return true;
}

// Translation table: RawHookEvent → the RawEvent used by normalization. Empty means unknown (drop).
[[nodiscard]] std::optional<recorder::RawEvent> translate(const RawHookEvent &e) {
    recorder::RawEvent r;
    r.time = e.time;

    if (e.source == kSourceKeyboard) {
        if (e.msg == WM_KEYDOWN || e.msg == WM_SYSKEYDOWN) {
            r.kind = recorder::RawKind::KeyDown;
        } else if (e.msg == WM_KEYUP || e.msg == WM_SYSKEYUP) {
            r.kind = recorder::RawKind::KeyUp;
        } else {
            return std::nullopt;
        }
        r.vk = e.vk;
        return r;
    }

    switch (e.msg) {
        case WM_MOUSEMOVE:
            r.kind = recorder::RawKind::Move;
            break;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
            r.kind = recorder::RawKind::ButtonDown;
            r.button = buttonOf(e);
            break;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
            r.kind = recorder::RawKind::ButtonUp;
            r.button = buttonOf(e);
            break;
        case WM_MOUSEWHEEL:
            r.kind = recorder::RawKind::Wheel;
            r.wheelDelta = static_cast<std::int16_t>(HIWORD(e.mouseData)); // signed
            r.horizontal = false;
            break;
        case WM_MOUSEHWHEEL:
            r.kind = recorder::RawKind::Wheel;
            r.wheelDelta = static_cast<std::int16_t>(HIWORD(e.mouseData));
            r.horizontal = true;
            break;
        default:
            return std::nullopt;
    }

    r.x = e.x;
    r.y = e.y;
    return r;
}

// ─────────────────────────────── pump ───────────────────────────────

// Move every event currently in the queue out: bulk take → decide → translate → append.
//   returning true means the raw count reached the early-stop threshold (the caller exits on it).
bool drainInto(CaptureState &state, moodycamel::ConcurrentQueue<RawHookEvent> &queue) {
    static thread_local std::array<RawHookEvent, kDrainBatch> batch{};

    std::size_t got = 0;
    while ((got = queue.try_dequeue_bulk(batch.data(), batch.size())) > 0) {
        for (std::size_t i = 0; i < got; ++i) {
            const RawHookEvent &e = batch[i];
            if (!accept(state, e)) continue;
            if (auto translated = translate(e)) state.raw.push_back(*translated);

            if (state.opts.maxActions > 0 &&
                state.raw.size() >= kRawPerAction * static_cast<std::size_t>(state.opts.maxActions)) {
                return true;
            }
        }
    }
    return false;
}

// Pump main loop. Only three exit conditions: stop signal received, raw limit reached, wait failed.
void pumpLoop(CaptureState &state, moodycamel::ConcurrentQueue<RawHookEvent> &queue, const std::stop_token &st) {
    RateLimiter progress{kProgressInterval};
    const auto started = ITimer::now();

    MSG msg{};
    bool limitReached = false;

    while (true) {
        // ① drain the queue first (filtering/translation live here, not in the callback)
        limitReached = drainInto(state, queue);

        // ② progress: RateLimiter allows at most one per second; the first call passes immediately
        if (progress.allow()) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(ITimer::now() - started).count();
            LOG_INFO("recorder: recording... {}s, raw={}, dropped injected={} packet={} "
                     "hotkey={} sampled={} queueFull={}",
                     elapsed,
                     state.raw.size(),
                     state.dropped.injected,
                     state.dropped.packet,
                     state.dropped.hotkey,
                     state.dropped.sampled,
                     t_droppedQueueFull);
        }

        // ③ raw limit reached: exact truncation is left to normalize
        if (limitReached) {
            LOG_WARN("recorder: action limit reached");
            break;
        }
        // ④ someone already requested stop (the second safety net besides stop_callback)
        if (st.stop_requested()) break;

        // ⑤ wait for input: returns at once with a message (≈0 delay), otherwise after 1s to log progress
        const DWORD waited = ::MsgWaitForMultipleObjects(
                0, nullptr, FALSE, static_cast<DWORD>(kProgressInterval.count()), QS_ALLINPUT);

        if (waited == WAIT_OBJECT_0) {
            // ⑥ take messages. the system calls the low-level hook callbacks right inside this
            //    (PeekMessage dispatches internally), so this step is "receiving input" itself.
            bool stopMessage = false;
            while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == kStopMessage) {
                    stopMessage = true;
                    break;
                }
            }
            if (stopMessage) break;
        } else if (waited == WAIT_FAILED) {
            // should not happen; if it does, treat it as stop so the loop cannot spin forever
            LOG_WARN("recorder: message wait failed");
            break;
        }
        // WAIT_TIMEOUT: do nothing, go back to ① (which is also the next progress time point)
    }

    // drain once more before exiting: events may still be left between the stop signal and the last enqueue
    static_cast<void>(drainInto(state, queue));
}

[[nodiscard]] recorder::NormalizeOptions makeNormalizeOptions(const recorder::RecordOptions &opts) {
    recorder::NormalizeOptions n;
    n.startKeyVk = opts.startKeyVk;
    n.stopKeyVk = opts.stopKeyVk;
    n.startMods = opts.startMods;
    n.stopMods = opts.stopMods;
    n.maxActions = opts.maxActions;
    return n;
}

} // namespace

// ============================================================================
// Public entry point
// ============================================================================

std::vector<Action> recorder::run(const RecordOptions &opts, const ITimer &timer, const std::stop_token &st) {
    // ── 1. start delay ─────────────────────────────────────────────────────
    // placed before installing hooks: nothing is intercepted during this wait, a few ms late does not matter;
    // the other way round, installing hooks first means "nothing recorded for 200ms while the system is hooked".
    // the main goal is letting the start hotkey's keys come up first, head/tail stripping is the second net.
    if (const int startDelayMs = std::max(0, opts.startDelayMs); startDelayMs > 0) {
        if (!timer.wait(ITimer::Duration{startDelayMs}, st)) {
            LOG_DEBUG("recorder: stopped during start delay");
            return {};
        }
    }

    // ── 2. make this thread own a message queue first ────────────────────────────────────────
    // a Windows thread message queue is created lazily; without one PostThreadMessage fails,
    // the stop signal never gets posted and StopRecord stays blocked in join forever.
    // touch it at minimal cost here: look only at WM_USER and do not remove the message.
    MSG msg{};
    ::PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    // ── 3. queue and callback entry ───────────────────────────────────────────────
    moodycamel::ConcurrentQueue<RawHookEvent> queue{kQueueCapacity}; // preallocated, zero allocation in callbacks
    t_droppedQueueFull = 0;
    t_queue = &queue;
    QueueGuard queueGuard; // any exit path from this function clears t_queue

    // ── 4. install both hooks ──────────────────────────────────────────────────
    // constructed in order: if the second fails the first is already removed by wil::unique_hhook,
    //   "either both installed or neither" holds structurally, no hand-written rollback needed.
    // hMod is our own EXE handle; low-level hook callbacks run on our own thread,
    //   so no extra DLL is needed (and none exists).
    const DWORD threadId = ::GetCurrentThreadId();

    wil::unique_hhook keyboardHook{::SetWindowsHookExW(WH_KEYBOARD_LL, &onKeyboard, ::GetModuleHandleW(nullptr), 0)};
    THROW_LAST_ERROR_IF_NULL(keyboardHook);

    wil::unique_hhook mouseHook{::SetWindowsHookExW(WH_MOUSE_LL, &onMouse, ::GetModuleHandleW(nullptr), 0)};
    THROW_LAST_ERROR_IF_NULL(mouseHook);

    // ── 5. stop callback: wake the pump blocked in the wait ──────────────────────────────
    // request_stop may happen at any moment (StopRecord on the UI thread), even right at registration --
    //   std::stop_callback runs the callback immediately when stop was already requested,
    //   so this message is already in the queue before the pump starts and the first take sees it.
    std::stop_callback onStop{st, [threadId] { ::PostThreadMessageW(threadId, kStopMessage, 0, 0); }};

    LOG_INFO("recorder: recording started (delay={}ms, moveInterval={}ms, ignoreInjected={}, "
             "maxActions={})",
             std::max(0, opts.startDelayMs),
             opts.moveIntervalMs,
             opts.ignoreInjected,
             opts.maxActions);

    // ── 6. pump: wait for input, drain the queue, log progress until stopped or the limit is hit ─────────────────
    CaptureState state{opts};
    pumpLoop(state, queue, st);

    // ── 7. teardown order (the order itself is the correctness) ────────────────────────────────
    // ① cut the callback entry first: from now on even a late callback never writes to the queue about to die
    // ② then remove the hooks: from here the system no longer calls our callbacks
    // ③ finally drain once (already done at the end of pumpLoop): an event may have been queued just before
    //    the hooks were removed and not yet taken by the pump; skipping this loses the tail
    t_queue = nullptr;
    mouseHook.reset();
    keyboardHook.reset();

    // ── 8. normalization ──────────────────────────────────────────────────────
    // pure function: head/tail modifier stripping, auto-repeat merge, click/drag decision, wheel accumulation,
    //   delayMs computation, exact maxActions truncation, all done here (not in the hook callback).
    std::vector<Action> actions = normalize(state.raw, makeNormalizeOptions(opts));

    LOG_INFO("recorder: recording finished, raw={}, actions={}, dropped injected={} packet={} "
             "hotkey={} sampled={} queueFull={}",
             state.raw.size(),
             actions.size(),
             state.dropped.injected,
             state.dropped.packet,
             state.dropped.hotkey,
             state.dropped.sampled,
             t_droppedQueueFull);

    // an empty list is a valid result: "no action recorded" is for the panel to message
    return actions;
}
