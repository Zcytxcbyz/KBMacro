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

// ============================================================
// 实现要点
//
//   - 公开方法只允许在 UI 线程调用，所以内部不加锁；越界由 wxASSERT_MSG 兜底
//   - 工作线程只干两件事：跑 body、把结果 notify 回 UI 线程
//   - notify 投递的 functor 会被事件对象拷贝一份，而且 Execute() 是无参调用
//     （wxAsyncMethodCallEventFunctor），所以数据全靠捕获带进去，
//     并且只许捕获 指针 / shared_ptr / 小 POD 这三类
//   - 状态日志：启动 / 停止 / 结束（含原因）/ 异常，都是低频，直接 LOG_*
//     启动参数由面板层记录，这里不重复
//   - modules/* 尚未接线：各 body 是占位实现，真实循环按 TODO(接线) 替换
// ============================================================

namespace {

    // scoped enum 不能直接交给 fmt（编译期错误），日志用这两个小函数转一下。
    // 只服务日志：不落盘、不做表驱动。关掉 ENABLE_LOGGING 时没人用它们。
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
// 连点器
// ------------------------------------------------------------

void ThreadManager::StartClicker(const ClickerParams &p, FinishCallback onFinished) {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StartClicker must be called from the UI thread");

    // timer 与参数都在这里按 UI 线程快照进闭包：
    // m_timerRef 只被读这一次，之后切计时器模式不影响已启动的任务
    launch(TaskKind::Clicker, std::move(onFinished),
           // ReSharper disable once CppLambdaCaptureNeverUsed
           [timer = m_timerRef, p](const std::stop_token& st) {
               // TODO(接线)：换成 modules/Clicker 的循环
               //   RunController rc(p.run, ITimer::now());
               //   while (rc.shouldRun(ITimer::now())) {
               //       发一次点击（win32/SendInput：每次记日志 + 检查返回值）；
               //       rc.noteIteration();
               //       if (!timer->wait(rc.clampWait(Duration(p.intervalMs), ITimer::now()), st)) break;
               //   }
               LOG_WARN("clicker: body not wired yet, finishing immediately"); // NOLINT(*-lambda-function-name)
               return nullptr;   // 非录制任务没有产物
           });
}

void ThreadManager::StopClicker() {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StopClicker must be called from the UI thread");
    stopTask(TaskKind::Clicker);
}

// ------------------------------------------------------------
// 键盘重复发送
// ------------------------------------------------------------

void ThreadManager::StartKeyboard(const KeyboardParams &p, FinishCallback onFinished) {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StartKeyboard must be called from the UI thread");

    launch(TaskKind::Keyboard, std::move(onFinished),
           // ReSharper disable once CppLambdaCaptureNeverUsed
           [timer = m_timerRef, p](const std::stop_token& st) {
               // TODO(接线)：换成 modules/Keyboard 的循环（与连点器同骨架，动作换成按下/抬起）
               LOG_WARN("keyboard: body not wired yet, finishing immediately"); // NOLINT(*-lambda-function-name)
               return nullptr;   // 非录制任务没有产物
           });
}

void ThreadManager::StopKeyboard() {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StopKeyboard must be called from the UI thread");
    stopTask(TaskKind::Keyboard);
}

// ------------------------------------------------------------
// 录制
// ------------------------------------------------------------

void ThreadManager::StartRecord(FinishCallback onFinished) {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StartRecord must be called from the UI thread");

    // 录制与回放互斥：后来者被拒绝，不抢占（正在回放的东西被顶掉更让人意外）
    if (m_slots[index(TaskKind::Playback)].state == TaskState::Running) {
        LOG_WARN("threadmanager: record rejected: playback is running");
        return;
    }

    launch(TaskKind::Record, std::move(onFinished), [](const std::stop_token& st) {
        // TODO(接线)：换成 modules/Recorder 的采集端
        //   起 WH_KEYBOARD_LL / WH_MOUSE_LL 钩子 → 回调只入列 ConcurrentQueue →
        //   消费端（本线程）出列、算 delayMs/holdMs、
        //   退出前必须 drain 光队列，再 return
        // 返回非空表：录制任务即使一个动作都没录到也要交回"空表"（见 TaskResult 注释）
        LOG_WARN("recorder: body not wired yet, finishing with empty result"); // NOLINT(*-lambda-function-name)
        return std::make_shared<std::vector<Action>>();
    });
}

void ThreadManager::StopRecord() {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StopRecord must be called from the UI thread");
    stopTask(TaskKind::Record);
}

// ------------------------------------------------------------
// 回放
// ------------------------------------------------------------

void ThreadManager::StartPlayback(std::vector<Action> actions, const RunSpec &run,
                                  FinishCallback onFinished) {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StartPlayback must be called from the UI thread");

    // 反向互斥：录制在跑时拒绝回放
    if (m_slots[index(TaskKind::Record)].state == TaskState::Running) {
        LOG_WARN("threadmanager: playback rejected: record is running");
        return;
    }

    // actions 按值收下后 move 进闭包：调用方（面板）的 m_actions 不受影响
    launch(TaskKind::Playback, std::move(onFinished),
           // ReSharper disable once CppLambdaCaptureNeverUsed
           [timer = m_timerRef, actions = std::move(actions), run](const std::stop_token& st) {
               // TODO(接线)：换成 modules/Recorder 的回放端（RunController + timer->wait + SendInput）；
               //             actions / run / timer 已经快照进来，直接用
               LOG_WARN("playback: body not wired yet, finishing immediately"); // NOLINT(*-lambda-function-name)
               return nullptr;   // 非录制任务没有产物
           });
}

void ThreadManager::StopPlayback() {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::StopPlayback must be called from the UI thread");
    stopTask(TaskKind::Playback);
}

// ------------------------------------------------------------
// 状态查询
// ------------------------------------------------------------

bool ThreadManager::IsRunning(const TaskKind kind) const {
    wxASSERT_MSG(wxIsMainThread(), "ThreadManager::IsRunning must be called from the UI thread");

    // state 是权威：Timed / Counted 跑完后线程已死，但它仍然 joinable，
    // 所以不能拿 joinable() 当"在跑"
    return m_slots[index(kind)].state == TaskState::Running;
}

// ------------------------------------------------------------
// 内部实现
// ------------------------------------------------------------

void ThreadManager::launch(TaskKind kind, FinishCallback userCb, ThreadBody body) {
    auto &[thread, state, generation] = m_slots[index(kind)];

    // 同一任务已在跑 → 忽略本次请求（拒绝而非抢占）。
    // 顺手停掉旧的再起是错的：那会静默丢掉旧任务的结果（比如正在录的内容），
    // 而旧任务的那条通知又会被序号校验丢弃，连"录制完成"的交付窗都不会出现。
    if (state == TaskState::Running) {
        LOG_DEBUG("threadmanager: task {} already running, start ignored", taskKindName(kind));
        return;
    }

    // 防御性回收：上一轮任务结束后线程已死，但仍是 joinable。
    // 必须在这里收掉，否则下面赋值会析构掉一个 joinable 的 jthread → std::terminate。
    // 走到这里 state 一定不是 Running，所以 join 立即返回。
    if (thread.joinable()) {
        thread.request_stop();
        thread.join();
    }

    // 序号只在这里推进。它用来丢弃"上一轮任务还压在 UI 队列里的结束通知"；
    // 若在 Stop 里也推进，就挡不住"定时任务跑完 + 用户抢先开始新任务"这一条。
    const unsigned gen = ++generation;

    state = TaskState::Running;
    LOG_INFO("threadmanager: task {} started: gen={}", taskKindName(kind), gen);

    // cb 装进 shared_ptr：CallAfter 会把 functor 拷进事件对象，别让闭包被反复拷贝
    auto cb = std::make_shared<FinishCallback>(std::move(userCb));

    // 工作线程 → UI 线程。notify 在工作线程里被调用；
    // 它内部的 lambda 会被 CallAfter 拷进事件对象，所以只捕获 指针 / shared_ptr / 小 POD
    auto notify = [this, kind, gen, cb](TaskResult res) {
        LOG_INFO("threadmanager: task {} finishing: gen={}, reason={}", // NOLINT(*-lambda-function-name)
                 taskKindName(kind), gen, finishReasonName(res.reason));

        m_ui.CallAfter([this, kind, gen, cb, res = std::move(res)]() mutable {
            Slot &s = m_slots[index(kind)];

            // 陈旧通知：属于上一轮任务，丢掉
            if (gen != s.generation) {
                LOG_DEBUG("threadmanager: stale result ignored: gen={} (current={})", gen, s.generation); // NOLINT(*-lambda-function-name)
                return;
            }

            s.state = TaskState::Stopped;      // 先落状态
            if (*cb) (*cb)(std::move(res));    // 再回调：面板在这里查 IsRunning 才是准的
        });
    };

    // body 的异常不逃出线程（否则 std::terminate），也不让状态永远卡在 Running
    thread = std::jthread{
        [kind, body = std::move(body), notify = std::move(notify)](const std::stop_token& st) {
            TaskResult res;

            try {
                res.recorded = body(st);
                res.reason = st.stop_requested() ? FinishReason::Stopped
                                                 : FinishReason::Completed;
            } catch (const std::exception &e) {
                LOG_ERROR("threadmanager: task {} threw: {}", taskKindName(kind), e.what()); // NOLINT(*-lambda-function-name)
                res = TaskResult{.reason = FinishReason::Failed};
            } catch (...) {
                LOG_ERROR("threadmanager: task {} threw: unknown exception", taskKindName(kind)); // NOLINT(*-lambda-function-name)
                res = TaskResult{.reason = FinishReason::Failed};
            }

            // 恰好一次：正常返回与抛异常两条路各一次
            notify(std::move(res));
        }
    };
}

void ThreadManager::stopTask(const TaskKind kind) {
    Slot &slot = m_slots[index(kind)];

    // 没起过 / 已经回收过 → 无事可做
    if (!slot.thread.joinable()) return;

    // 阻塞 join：用户按了停止就不能"还没停"。Sleep 计时器上界约 20ms，Spin 亚毫秒，
    // 这点 UI 停顿可以接受；把 join 推迟到下次 Start 是错的。
    slot.thread.request_stop();
    slot.thread.join();

    slot.state = TaskState::Stopped;
    LOG_INFO("threadmanager: task {} stopped", taskKindName(kind));
}

void ThreadManager::StopAll() {
    LOG_INFO("threadmanager: stop all");

    // 先全部叫停：并行等待，关窗口从 4×20ms 降到 ~20ms
    for (auto &slot: m_slots) {
        if (slot.thread.joinable()) slot.thread.request_stop();
    }

    // 再逐个回收
    for (auto &slot: m_slots) {
        if (!slot.thread.joinable()) continue;
        slot.thread.join();
        slot.state = TaskState::Stopped;
    }
}
