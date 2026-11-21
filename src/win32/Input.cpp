#include "win32/Input.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <windows.h>

#include "common/Log.hpp"

// ============================================================
// Implementation notes
//
// Coordinates: SendInput's absolute coordinate is a normalized 0-65535 value, which together with
//   MOUSEEVENTF_VIRTUALDESK maps onto the whole virtual desktop (multi-monitor, negative coords included),
//   so it is first converted with SM_X/YVIRTUALSCREEN + SM_CX/CYVIRTUALSCREEN.
//   the divisor is "width - 1": 65535 maps to the last pixel; with "width" the rightmost/bottom row would
//   land outside the screen and miss. MulDiv rounds, half a pixel more accurate than integer division.
//
// Keyboard: fill only wVk, no wScan, no KEYEVENTF_EXTENDEDKEY. the system translates the VK into a
//   scan code itself; filling a scan code by hand without the extended prefix makes scan-code readers
//   pick the wrong key instead. patch for a specific program (old games etc.) only if it really rejects it.
//
// Marker: every event carries dwExtraInfo = kInjectedSignature. recorder's low-level hook receives
//   our own injected events (most visible during playback) and filters them out by this value, otherwise
//   recording while playing would record itself.
// ============================================================

namespace {
// Same value as the public constant in win32/Input.hpp; this is just an alias
// so makeMouse / makeKeyboard below do not need the qualified name every time.
constexpr ULONG_PTR kInjectedSignature = input::kInjectedSignature;

// Dry-run mode: with ENABLE_DRY_RUN defined it only logs and never calls SendInput --
// the call is not even compiled in, so it has zero effect on the system.
// apart from "send or not" the behavior matches the real mode exactly: it still waits holdMs, logs,
// and returns true, the log format just gains a dry flag.
#ifdef KBMACRO_DRY_RUN
constexpr bool kDryRun = true;
#else
constexpr bool kDryRun = false;
#endif

// Modifier press order: Ctrl -> Shift -> Alt -> Win; reversed on release.
struct ModifierKey {
    unsigned mask;
    WORD vk;
};

constexpr ModifierKey kModifierKeys[] = {
        {.mask = hk::kModCtrl, .vk = VK_CONTROL},
        {.mask = hk::kModShift, .vk = VK_SHIFT},
        {.mask = hk::kModAlt, .vk = VK_MENU},
        {.mask = hk::kModWin, .vk = VK_LWIN},
};

// Build a mouse event. The struct must be fully zeroed (value-initialized).
[[nodiscard]] INPUT makeMouse(const DWORD flags, const LONG dx = 0, const LONG dy = 0, const DWORD data = 0) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    in.mi.mouseData = data;
    in.mi.dwFlags = flags;
    in.mi.dwExtraInfo = kInjectedSignature;
    return in;
}

[[nodiscard]] INPUT makeKeyboard(const WORD vk, const DWORD flags) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = flags;
    in.ki.dwExtraInfo = kInjectedSignature;
    return in;
}

// The down/up flags for each mouse button, plus the side-button index.
struct ButtonFlags {
    DWORD down;
    DWORD up;
    DWORD data;
};

[[nodiscard]] constexpr ButtonFlags buttonFlags(const MouseButton button) noexcept {
    switch (button) {
        case MouseButton::Left:
            return {.down = MOUSEEVENTF_LEFTDOWN, .up = MOUSEEVENTF_LEFTUP, .data = 0};
        case MouseButton::Right:
            return {.down = MOUSEEVENTF_RIGHTDOWN, .up = MOUSEEVENTF_RIGHTUP, .data = 0};
        case MouseButton::Middle:
            return {.down = MOUSEEVENTF_MIDDLEDOWN, .up = MOUSEEVENTF_MIDDLEUP, .data = 0};
        case MouseButton::Side1:
            return {.down = MOUSEEVENTF_XDOWN, .up = MOUSEEVENTF_XUP, .data = XBUTTON1};
        case MouseButton::Side2:
            return {.down = MOUSEEVENTF_XDOWN, .up = MOUSEEVENTF_XUP, .data = XBUTTON2};
    }
    return {.down = MOUSEEVENTF_LEFTDOWN, .up = MOUSEEVENTF_LEFTUP, .data = 0};
}

// Keyboard event sequence: on press modifiers lead, then the main key; on release main key leads, then modifiers.
// when no main key is set (vk == 0) only the modifiers are sent; if neither exists an empty list is returned.
[[nodiscard]] std::vector<INPUT> keyEvents(const Hotkey &hk, const bool down) {
    const DWORD flags = down ? 0u : KEYEVENTF_KEYUP;

    std::vector<INPUT> modifiers;
    for (const auto &[mask, vk] : kModifierKeys) {
        if ((hk.modifiers & mask) != 0) {
            modifiers.push_back(makeKeyboard(vk, flags));
        }
    }

    std::vector<INPUT> main;
    if (hk.vk != 0) {
        main.push_back(makeKeyboard(static_cast<WORD>(hk.vk), flags));
    }

    if (down) {
        modifiers.insert(modifiers.end(), main.begin(), main.end());
        return modifiers;
    }
    main.insert(main.end(), modifiers.begin(), modifiers.end());
    return main;
}

// Describe a key combination for the log.
[[nodiscard]] std::string keyLabel(const Hotkey &hk) {
    return "vk=" + std::to_string(hk.vk) + " mods=" + std::to_string(hk.modifiers);
}

// Send a batch of events: one SendInput = one DEBUG line.
// failure (return value below expected) -> one WARN line -> replay cleanup (release-type events) -> throw.
//   what    : action description for the log and the exception
//   cleanup : events replayed after a failure; nullptr = nothing to clean up (move, wheel)
void sendEvents(std::vector<INPUT> events, std::string what, std::vector<INPUT> *cleanup = nullptr) {
    if (events.empty()) {
        LOG_DEBUG("input: {} skipped, no events", what);
        return;
    }

    const auto expected = static_cast<UINT>(events.size());

    // dry run: no SendInput call, assume everything was inserted
    // ReSharper disable once CppInitializedValueIsAlwaysRewritten
    UINT sent = expected;
    if constexpr (!kDryRun) {
        sent = ::SendInput(expected, events.data(), static_cast<int>(sizeof(INPUT)));
    }

    if (sent == expected) {
        LOG_DEBUG("input: {} (sent={}/{}, dry={})", what, sent, expected, static_cast<int>(kDryRun));
        return;
    }

    // insertion fails for basically two reasons: the foreground window has higher integrity (UIPI blocks
    // the injection), or BlockInput is active. both are persistent, retrying is pointless.
    const DWORD error = ::GetLastError();
    LOG_WARN("input: {} failed: sent={}/{}, GetLastError={}", what, sent, expected, error);

    if (cleanup != nullptr && !cleanup->empty()) {
        const auto cleanupCount = static_cast<UINT>(cleanup->size());
        const UINT cleaned = ::SendInput(cleanupCount, cleanup->data(), static_cast<int>(sizeof(INPUT)));
        if (cleaned != cleanupCount) {
            LOG_ERROR("input: {} cleanup failed: sent={}/{}, GetLastError={}",
                      what,
                      cleaned,
                      cleanupCount,
                      ::GetLastError());
        }
    }

    throw input::SendInputError(what + " failed: sent=" + std::to_string(sent) + "/" + std::to_string(expected) +
                                ", GetLastError=" + std::to_string(error));
}
} // namespace

namespace input {

void init() {
    const int x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

    int dpi = 0;
    if (const HDC dc = ::GetDC(nullptr); dc != nullptr) { // NOLINT(*-misplaced-const)
        dpi = ::GetDeviceCaps(dc, LOGPIXELSX);
        ::ReleaseDC(nullptr, dc);
    }

    LOG_DEBUG("input: virtual screen at ({}, {}), {}x{}, dpi={}", x, y, w, h, dpi);

    if constexpr (kDryRun) {
        LOG_WARN("input: DRY RUN enabled, nothing will be injected");
    }
}

void moveTo(const int x, const int y) {
    const int screenX = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int screenY = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int screenW = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int screenH = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // 65535 maps to the last pixel -> the divisor is width/height - 1; a 1-pixel edge case falls back to 1.
    const int spanW = (screenW > 1) ? screenW - 1 : 1;
    const int spanH = (screenH > 1) ? screenH - 1 : 1;

    const int normX = ::MulDiv(x - screenX, 65535, spanW);
    const int normY = ::MulDiv(y - screenY, 65535, spanH);

    std::string what = "move to (" + std::to_string(x) + ", " + std::to_string(y) + ")";
    if constexpr (kDryRun) {
        // in dry run also log the normalized result: no need to recompute the formula by hand when coords are off
        what += " norm=(" + std::to_string(normX) + ", " + std::to_string(normY) + ")";
    }

    sendEvents({makeMouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, normX, normY)},
               std::move(what));
}

void moveBy(const int dx, const int dy) {
    // relative move: send MOUSEEVENTF_MOVE only, without ABSOLUTE / VIRTUALDESK,
    // dx/dy are physical pixel deltas (the system clamps them at the screen edge).
    sendEvents({makeMouse(MOUSEEVENTF_MOVE, dx, dy)},
               "move by (" + std::to_string(dx) + ", " + std::to_string(dy) + ")");
}

void buttonDown(const MouseButton button) {
    const auto [down, up, data] = buttonFlags(button);
    // on a failed press replay one release: harmless whether nothing went down or only part of it did.
    std::vector<INPUT> cleanup{makeMouse(up, 0, 0, data)};
    sendEvents({makeMouse(down, 0, 0, data)}, std::string(mouseButtonToString(button)) + " down", &cleanup);
}

void buttonUp(const MouseButton button) {
    const ButtonFlags flags = buttonFlags(button);
    std::vector<INPUT> retry{makeMouse(flags.up, 0, 0, flags.data)};
    sendEvents({makeMouse(flags.up, 0, 0, flags.data)}, std::string(mouseButtonToString(button)) + " up", &retry);
}

void wheel(const WheelDirection direction, const int steps) {
    const bool vertical = (direction == WheelDirection::Up || direction == WheelDirection::Down);
    const bool negative = (direction == WheelDirection::Down || direction == WheelDirection::Left);
    const LONG delta = steps * WHEEL_DELTA;

    // vertical uses MOUSEEVENTF_WHEEL (positive = up), horizontal uses MOUSEEVENTF_HWHEEL (positive = right).
    // multiple steps accumulate into one event's mouseData, one SendInput per log line.
    sendEvents({makeMouse(vertical ? MOUSEEVENTF_WHEEL : MOUSEEVENTF_HWHEEL,
                          0,
                          0,
                          static_cast<DWORD>(negative ? -delta : delta))},
               std::string(wheelDirectionToString(direction)) + " x" + std::to_string(steps));
}

void keyDown(const Hotkey &hk) {
    std::vector<INPUT> events = keyEvents(hk, true);
    std::vector<INPUT> cleanup = keyEvents(hk, false);
    sendEvents(std::move(events), "key down " + keyLabel(hk), &cleanup);
}

void keyUp(const Hotkey &hk) {
    std::vector<INPUT> events = keyEvents(hk, false);
    std::vector<INPUT> retry = events; // on a failed release replay one more release
    sendEvents(std::move(events), "key up " + keyLabel(hk), &retry);
}

bool click(const MouseButton button, const int holdMs, const ITimer &timer, const std::stop_token &st) {
    if (st.stop_requested()) return false;

    buttonDown(button);
    // the hold duration runs on the timer. release even when interrupted: never leave a mouse button down.
    const bool completed = timer.wait(ITimer::Duration{holdMs}, st);
    buttonUp(button);
    return completed;
}

bool pressKey(const Hotkey &hk, const int holdMs, const ITimer &timer, const std::stop_token &st) {
    if (st.stop_requested()) return false;

    keyDown(hk);
    const bool completed = timer.wait(ITimer::Duration{holdMs}, st);
    keyUp(hk);
    return completed;
}

} // namespace input
