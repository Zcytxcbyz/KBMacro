#include "managers/ThreadManager.hpp"

#include <exception>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <wx/debug.h>
#include <wx/event.h>
#include <wx/thread.h>

#include "common/Log.hpp"
#include "modules/Clicker.hpp"
#include "modules/Keyboard.hpp"
#include "modules/Playback.hpp"

// ============================================================
// Implementation notes
//
//   - public methods may only be called on the UI thread, so no internal locking; wxASSERT_MSG catches misuse
//   - the worker thread does only two things: run the body, notify the result back to the UI thread
//   - the functor posted by notify is copied into the event object, and Execute() takes no arguments
//     (wxAsyncMethodCallEventFunctor), so all data has to be carried in through the capture,
//     and only these three kinds may be captured: pointer / shared_ptr / small POD
//   - state logs: start / stop / finish (with reason) / exception, all low frequency, straight to LOG_*
//     start params are logged by the panel layer, not repeated here
//   - modules/* not wired up yet: each body is a placeholder; the real loop replaces TODO(wiring)
// ============================================================

namespace {

    // a scoped enum cannot be handed to fmt directly (compile error), these two helpers convert it for logging.
    // logging only: no file output, no table-driven lookup. nobody uses them when ENABLE_LOGGING is off.
    [[maybe_unused]] [[nodiscard]] constexpr std::string_view taskKindName(const TaskKind kind) noexcept {
        switch (kind) {
            case TaskKind::Clicker:  return "clicker";
            case TaskKind::Keyboard: return "keyboard";
            case TaskKind::Record:   return "record";
            case TaskKind::Playback: return "playback";
        }
        return "unknown";
    }

    [[maybe_unused]] [[nodiscard]] constexpr std::string_view finishReasonName(const FinishReason reason) noexcept {
        switch (reason) {
            case FinishReason::Completed: return "completed";
            case FinishReason::Stopped:   return "stopped";
            case FinishReason::Failed:    return "failed";
        }
        return "unknown";
    }

} // namespace

ThreadManager::ThreadManager(std::shared_ptr<ITimer> &timerRef, wxEvtHandler &ui)
    : m_timerRef(timerRef),
      m_ui(ui) {
}

ThreadManager::~ThreadManager() {
    StopAll();
}

// ------------------------------------------------------------
// Clicker
// ------------------------------------------------------------

void ThreadManager::StartClicker(const ClickerParams &p, FinishCallback onFinished) {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StartClicker must be called from the UI thread");

    // timer and params are snapshotted into the closure here, on the UI thread:
    // m_timerRef is read only this once, switching timer mode later does not affect already started tasks
    launch(TaskKind::Clicker, std::move(onFinished),
           [timer = m_timerRef, p](const std::stop_token& st) {
               // the loop skeleton lives in modules/Clicker; here only params and timer are handed over.
               // on stop, clicker::run finishes up on its own (releases stuck keys) before returning.
               const int executed = clicker::run(p, *timer, st);
               return TaskResult{.executed = executed};
           });
}

void ThreadManager::StopClicker() {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StopClicker must be called from the UI thread");
    stopTask(TaskKind::Clicker);
}

// ------------------------------------------------------------
// Keyboard repeat
// ------------------------------------------------------------

void ThreadManager::StartKeyboard(const KeyboardParams &p, FinishCallback onFinished) {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StartKeyboard must be called from the UI thread");

    launch(TaskKind::Keyboard, std::move(onFinished),
           [timer = m_timerRef, p](const std::stop_token& st) {
               // each round: press → wait holdMs → release; on stop, pressKey already compensates the release
               const int executed = keyboard::run(p, *timer, st);
               return TaskResult{.executed = executed};
           });
}

void ThreadManager::StopKeyboard() {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StopKeyboard must be called from the UI thread");
    stopTask(TaskKind::Keyboard);
}

// ------------------------------------------------------------
// Recording
// ------------------------------------------------------------

void ThreadManager::StartRecord(const recorder::RecordOptions &opts, FinishCallback onFinished) {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StartRecord must be called from the UI thread");

    // recording and playback are mutually exclusive: the latecomer is rejected, not preempted
    // (kicking out a running playback would be more surprising)
    if (m_slots[index(TaskKind::Playback)].state == TaskState::Running) {
        LOG_WARN("threadmanager: record rejected: playback is running");
        return;
    }

    // opts is captured by value: it is a local of the caller (panel), and the thread outlives the call stack
    launch(TaskKind::Record, std::move(onFinished),
           [timer = m_timerRef, opts](const std::stop_token &st) {
               // recording has no "natural end": either the user presses stop or the action limit is reached.
               //   executed is always 0 for recording (there is no "round" concept), the actions are in recorded:
               //   non-null pointer + empty vector = recorded, but nothing was captured (the panel hints on this)
               //   if the hook cannot be installed, recorder::run throws; launch records Failed, the panel reports it
               std::vector<Action> actions = recorder::run(opts, *timer, st);
               const auto count = static_cast<int>(actions.size());
               LOG_DEBUG("threadmanager: record finished, {} action(s)", count); // NOLINT(*-lambda-function-name)
               return TaskResult{
                   .recorded = std::make_shared<std::vector<Action>>(std::move(actions))};
           });
}

void ThreadManager::StopRecord() {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StopRecord must be called from the UI thread");
    stopTask(TaskKind::Record);
}

// ------------------------------------------------------------
// Playback
// ------------------------------------------------------------

void ThreadManager::StartPlayback(std::vector<Action> actions, const RunSpec &run,
                                  FinishCallback onFinished) {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StartPlayback must be called from the UI thread");

    // reverse exclusion: reject playback while recording is running
    if (m_slots[index(TaskKind::Record)].state == TaskState::Running) {
        LOG_WARN("threadmanager: playback rejected: record is running");
        return;
    }

    // actions is taken by value and then moved into the closure: the caller's (panel's) m_actions is untouched
    launch(TaskKind::Playback, std::move(onFinished),
           // ReSharper disable once CppLambdaCaptureNeverUsed
           [timer = m_timerRef, actions = std::move(actions), run](const std::stop_token &st) {
               const int executed = playback::run(actions, run, *timer, st);
               return TaskResult{.executed = executed};
           });
}

void ThreadManager::StopPlayback() {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StopPlayback must be called from the UI thread");
    stopTask(TaskKind::Playback);
}

// ------------------------------------------------------------
// State queries
// ------------------------------------------------------------

bool ThreadManager::IsRunning(const TaskKind kind) const {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::IsRunning must be called from the UI thread");

    // state is authoritative: after a Timed / Counted run the thread is dead but still joinable,
    // so joinable() must not be used as "is running"
    return m_slots[index(kind)].state == TaskState::Running;
}

// ------------------------------------------------------------
// Internals
// ------------------------------------------------------------

void ThreadManager::launch(TaskKind kind, FinishCallback userCb, ThreadBody body) {
    auto &[thread, state, generation] = m_slots[index(kind)];

    // same task already running → ignore this request (reject instead of preempt).
    // stopping the old one on the way is wrong: it would silently drop the old result (e.g. what is being recorded),
    // and that notification is dropped by the generation check, so the "recording finished" window never appears.
    if (state == TaskState::Running) {
        LOG_DEBUG("threadmanager: task {} already running, start ignored", taskKindName(kind));
        return;
    }

    // defensive reclaim: after the previous run the thread is dead but still joinable.
    // it must be reaped here, otherwise the assignment below destroys a joinable jthread → std::terminate.
    // state is never Running here, so join returns immediately.
    // hitting this means the previous notification is still queued on the UI thread (generation not yet advanced)
    if (thread.joinable()) {
        LOG_DEBUG("threadmanager: task {} reaped before start, gen={}",
                  taskKindName(kind), generation);
        thread.request_stop();
        thread.join();
    }

    // generation is advanced only here. it drops "finish notifications of the previous run still in the UI queue";
    // advancing it in Stop too would not cover "timed task finished + user already started a new task".
    const unsigned gen = ++generation;

    state = TaskState::Running;
    LOG_INFO("threadmanager: task {} started: gen={}", taskKindName(kind), gen);

    // cb goes into a shared_ptr: CallAfter copies the functor into the event object, avoid repeated closure copies
    auto cb = std::make_shared<FinishCallback>(std::move(userCb));

    // worker thread → UI thread. notify is called on the worker thread;
    // CallAfter copies its inner lambda into the event, so capture only pointer / shared_ptr / small POD
    auto notify = [this, kind, gen, cb](TaskResult res) {
        LOG_INFO("threadmanager: task {} finishing: gen={}, reason={}", // NOLINT(*-lambda-function-name)
                 taskKindName(kind), gen, finishReasonName(res.reason));

        m_ui.CallAfter([this, kind, gen, cb, res = std::move(res)]() mutable {
            Slot &s = m_slots[index(kind)];

            // stale notification: belongs to the previous run, drop it
            if (gen != s.generation) {
                LOG_DEBUG("threadmanager: stale result ignored: gen={} (current={})", gen, s.generation); // NOLINT(*-lambda-function-name)
                return;
            }

            s.state = TaskState::Stopped;      // set the state first
            if (*cb) (*cb)(std::move(res));    // then callback: IsRunning queried by the panel is accurate here
        });
    };

    // body exceptions must not escape the thread (std::terminate), nor leave the state stuck at Running forever
    thread = std::jthread{
        [kind, body = std::move(body), notify = std::move(notify)](const std::stop_token& st) {
            TaskResult res;

            try {
                res = body(st);
                res.reason = st.stop_requested() ? FinishReason::Stopped
                                                 : FinishReason::Completed;
            } catch (const std::exception &e) {
                LOG_ERROR("threadmanager: task {} threw: {}", taskKindName(kind), e.what()); // NOLINT(*-lambda-function-name)
                res = TaskResult{.reason = FinishReason::Failed};
            } catch (...) {
                LOG_ERROR("threadmanager: task {} threw: unknown exception", taskKindName(kind)); // NOLINT(*-lambda-function-name)
                res = TaskResult{.reason = FinishReason::Failed};
            }

            // exactly once: one path for normal return, one for the exception
            notify(std::move(res));
        }
    };
}

void ThreadManager::stopTask(const TaskKind kind) {
    Slot &slot = m_slots[index(kind)];

    // never started / already reaped → nothing to do
    if (!slot.thread.joinable()) {
        LOG_DEBUG("threadmanager: task {} not running, stop ignored", taskKindName(kind));
        return;
    }

    // blocking join: after the user presses stop it must not stay "not stopped yet".
    // Sleep timer ~20ms upper bound, Spin sub-millisecond, this UI pause is acceptable;
    // deferring the join to the next Start is wrong.
    slot.thread.request_stop();
    slot.thread.join();

    slot.state = TaskState::Stopped;
    LOG_INFO("threadmanager: task {} stopped", taskKindName(kind));
}

void ThreadManager::StopAll() {
    LOG_INFO("threadmanager: stop all");

    // signal all first: wait in parallel, closing the window drops from 4×20ms to ~20ms
    for (auto &slot: m_slots) {
        if (slot.thread.joinable()) slot.thread.request_stop();
    }

    // then reap one by one
    for (auto &slot: m_slots) {
        if (!slot.thread.joinable()) continue;
        slot.thread.join();
        slot.state = TaskState::Stopped;
    }
}
