#pragma once

#include <array>
// ReSharper disable once CppUnusedIncludeDirective
#include <cstddef>
#include <functional>
#include <memory>
#include <stop_token>
#include <thread>
#include <vector>

#include "common/Action.hpp"
#include "common/ClickerParams.hpp"
#include "common/ITimer.hpp"
#include "common/KeyboardParams.hpp"
#include "common/Types.hpp"

#include "modules/Recorder.hpp"

// only stores a reference and calls CallAfter in the .cpp, the header needs no full wx definition
class wxEvtHandler;

// ============================================================
// Task thread manager
//
// Responsibilities (single authority for thread lifetime and task state):
//   - start/stop threads (blocking join), hold the authoritative state, post "task finished" to the UI thread
//
// Not responsible for:
//   - knows no panel and does no business logic; no coordinate conversion (done at import/delivery)
//   - no UI text (the callback belongs to the panel); no StartAll (hotkeys are split in MainFrame)
//   - no SendOnce (the one-shot keyboard send has no thread and does not go through here)
//
// Threading:
//   - all public methods are called on the UI thread only, so no internal locking (wxASSERT_MSG as a net)
//   - finish notification: notify posts from the worker thread, the callback runs on the UI thread
//   - wxEvtHandler::CallAfter only has a const T& overload and copies the functor into the event,
//     so only capture pointers / shared_ptr / small PODs when posting
//
// State model (one Slot per task: thread + state + generation):
//   - state: Running is set only in launch; Stopped is set on the stop path and on the
//     UI side of the notification callback (idempotent, repeated sets have no effect)
//   - generation: only advanced in launch, never by Stop. Used to drop stale notifications--
//     when a finished timed-task notification is still queued while the user already clicked a new start,
//     that old notification must not mark the running new task as stopped
//
// Conflict handling (reject instead of preempt):
//   - Start again while the same task is running -> ignore this request (LOG_DEBUG), Stop first to change params
//   - recording <-> playback are mutually exclusive -> the latecomer is rejected (LOG_WARN), the other is not stopped
//
// Lifetime:
//   - MainFrame member (unique_ptr), must be constructed before the three panels and destroyed after them
//   - reset() first in ~MainFrame; the destructor calls StopAll(): signal all first, then join one by one
// ============================================================

// Task kind. The value is also used as the m_slots index: do not reorder, do not insert.
enum class TaskKind {
    Clicker,
    Keyboard,
    Record,
    Playback,
};

// Task finish report.
//   executed                  -> how many rounds this run actually executed
//                               clicker/keyboard: number of clicks (key presses); playback: actions played;
//                               not applicable to recording, always 0
//   recorded == nullptr       -> not a recording task
//   recorded non-null but ->empty() -> recording task, but no action was captured
struct TaskResult {
    FinishReason reason = FinishReason::Completed;
    int executed = 0;
    std::shared_ptr<std::vector<Action>> recorded;
};

// Finish callback. Finally executed on the UI thread; copyable at declaration (usually a thin closure in panels).
using FinishCallback = std::function<void(TaskResult)>;

class ThreadManager {
public:
    // timerRef: reference to the authoritative shared_ptr<ITimer> held by MainFrame.
    //           read only once at "start thread" (snapshotted into the thread closure),
    //           so switching timer mode later does not affect already started tasks.
    // ui      : event handler (MainFrame) that receives the "task finished" message.
    ThreadManager(std::shared_ptr<ITimer> &timerRef, wxEvtHandler &ui);
    ~ThreadManager();

    // copying / moving is deleted: holds threads, state and a UI reference, no value semantics
    ThreadManager(const ThreadManager &) = delete;
    ThreadManager &operator=(const ThreadManager &) = delete;
    ThreadManager(ThreadManager &&) = delete;
    ThreadManager &operator=(ThreadManager &&) = delete;

    // ---- clicker ----
    // params are copied into the thread by value; cb runs once on the UI thread when finished
    void StartClicker(const ClickerParams &p, FinishCallback onFinished);
    void StopClicker();

    // ---- keyboard repeat ----
    void StartKeyboard(const KeyboardParams &p, FinishCallback onFinished);
    void StopKeyboard();

    // ---- recording ----
    // opts is read on the UI thread (four config values + vk/mods of the start/stop hotkeys) and passed in:
    //   the thread does not read ConfigManager (singleton, not thread-safe) nor touch HotkeyManager.
    // playback is running -> reject: log LOG_WARN, do not start and do not callback
    void StartRecord(const recorder::RecordOptions &opts, FinishCallback onFinished);
    void StopRecord();

    // ---- playback ----
    // actions is taken by value: the caller passes the UI-side m_actions (one copy), moved into the thread.
    // coordinate conversion is already done at import/delivery, only the final result is taken here.
    // recording is running -> reject: log LOG_WARN, do not start and do not callback
    void StartPlayback(std::vector<Action> actions, const RunSpec &run, FinishCallback onFinished);
    void StopPlayback();

    // whether this task is currently running (used by panels to refresh button state)
    [[nodiscard]] bool IsRunning(TaskKind kind) const;

private:
    enum class TaskState {
        Stopped,
        Running,
    };

    // run record of a single task: the three fields live and die together, no index mismatch
    struct Slot {
        std::jthread thread;
        TaskState state = TaskState::Stopped;
        unsigned generation = 0;
    };

    // thread body: takes a stop_token and returns the result of this run (output + executed rounds).
    // internal data must be fully drained here (Recorder has to drain the whole queue) before return.
    // reason is not computed here--launch overrides it by st.stop_requested(), an exception records Failed.
    using ThreadBody = std::function<TaskResult(std::stop_token)>;

    static constexpr std::size_t kTaskCount = 4; // matches the number of TaskKind enumerators

    static_assert(static_cast<std::size_t>(TaskKind::Playback) + 1 == kTaskCount,
                  "When adding enum values to TaskKind, you need to update kTaskCount as well");

    [[nodiscard]] static constexpr std::size_t index(TaskKind kind) noexcept { return static_cast<std::size_t>(kind); }

    // single entry for starting a thread: conflict check -> defensive reclaim -> bump generation -> set Running ->
    // wrap in try/catch -> finish notification (exactly once)
    void launch(TaskKind kind, FinishCallback userCb, ThreadBody body);

    // shared implementation of StopXxx: request_stop + blocking join + set Stopped
    void stopTask(TaskKind kind);

    // called by ~ThreadManager only: signal all first, then join one by one
    void StopAll();

    std::shared_ptr<ITimer> &m_timerRef; // reference; read only on the UI thread
    wxEvtHandler &m_ui;

    std::array<Slot, kTaskCount> m_slots{};
};
