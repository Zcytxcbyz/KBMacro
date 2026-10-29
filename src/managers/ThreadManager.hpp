#pragma once

#include <array>
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

// 只存引用、只在 .cpp 里调 CallAfter，头文件不需要 wx 的完整定义
class wxEvtHandler;

// ============================================================
// 任务线程管理器
//
// 职责（线程生命周期与任务状态的唯一权威）：
//   - 起线程、停线程（阻塞 join）、持权威状态、把"任务结束"送回 UI 线程
//
// 不做：
//   - 不认识任何面板，不做业务；不做坐标换算（导入/交付那一刻已完成）
//   - 不做 UI 文案（cb 归面板）；不提供 StartAll（热键在 MainFrame 里分解）
//   - 不管 SendOnce（键盘的一次性发送没有线程，不走这里）
//
// 线程：
//   - 所有公开方法只在 UI 线程调用，因此内部不加锁（wxASSERT_MSG 兜底）
//   - 结束通知：notify 在工作线程里投递，cb 在 UI 线程执行
//   - wxEvtHandler::CallAfter 只有 const T& 重载、事件内部会拷贝 functor，
//     所以投递时只许捕获 指针 / shared_ptr / 小 POD
//
// 状态模型（每个任务一个 Slot：thread + state + generation）：
//   - state：Running 只在 launch 里置位；Stopped 在 stop 路径与通知回调的
//     UI 侧置位（幂等，重复置位无副作用）
//   - generation：只在 launch 推进，Stop 绝不推进。用来丢弃陈旧通知——
//     "定时任务跑完的通知还在队列里、用户已抢先点了新的开始"时，
//     那条旧通知不能把正在跑的新任务刷成已停止
//
// 冲突处理（拒绝而非抢占）：
//   - 同一任务已在跑时再 Start → 忽略本次请求（LOG_DEBUG），换参数请先 Stop
//   - 录制 ↔ 回放互斥 → 后来者被拒绝（LOG_WARN），不自动停对方
//
// 生命周期：
//   - MainFrame 成员（unique_ptr），必须在三个面板之前构造、之后销毁
//   - ~MainFrame 里先 reset()；析构函数内部 StopAll()：先全部叫停，再逐个 join
// ============================================================

// 任务种类。值同时用作 m_slots 的下标：别改顺序、别插值。
enum class TaskKind {
    Clicker,
    Keyboard,
    Record,
    Playback,
};

// 任务结束上报。
//   executed                  → 本次实际执行了多少轮
//                               连点器/键盘：点击（按键）次数；回放：播了几个动作；
//                               录制不适用，恒为 0
//   recorded == nullptr       → 非录制任务
//   recorded 非空但 ->empty() → 录制任务，但一个动作都没录到
struct TaskResult {
    FinishReason reason = FinishReason::Completed;
    int executed = 0;
    std::shared_ptr<std::vector<Action>> recorded;
};

// 结束回调。最终在 UI 线程执行；声明期可拷贝（面板里通常是薄闭包）。
using FinishCallback = std::function<void(TaskResult)>;

class ThreadManager {
public:
    // timerRef：MainFrame 持有的权威 shared_ptr<ITimer> 的引用。
    //           只有"启动线程"那一刻读一次（快照进线程闭包），
    //           所以之后切计时器模式不影响已启动的任务。
    // ui      ：接收"任务结束"消息的事件处理器（MainFrame）。
    ThreadManager(std::shared_ptr<ITimer> &timerRef, wxEvtHandler &ui);
    ~ThreadManager();

    // 禁止拷贝 / 移动：持线程、状态与 UI 引用，没有值语义
    ThreadManager(const ThreadManager &) = delete;
    ThreadManager &operator=(const ThreadManager &) = delete;
    ThreadManager(ThreadManager &&) = delete;
    ThreadManager &operator=(ThreadManager &&) = delete;

    // ---- 连点器 ----
    // 参数按值拷进线程；cb 在结束时于 UI 线程执行一次
    void StartClicker(const ClickerParams &p, FinishCallback onFinished);
    void StopClicker();

    // ---- 键盘重复发送 ----
    void StartKeyboard(const KeyboardParams &p, FinishCallback onFinished);
    void StopKeyboard();

    // ---- 录制 ----
    // 回放正在跑 → 拒绝：打 LOG_WARN，不启动也不回调
    void StartRecord(FinishCallback onFinished);
    void StopRecord();

    // ---- 回放 ----
    // actions 按值收：调用方传 UI 侧 m_actions（拷一次），内部 move 进线程。
    // 坐标换算已在导入/交付那一刻做完，这里只收最终结果。
    // 录制正在跑 → 拒绝：打 LOG_WARN，不启动也不回调
    void StartPlayback(std::vector<Action> actions, const RunSpec &run,
                       FinishCallback onFinished);
    void StopPlayback();

    // 该任务当前是否在跑（面板刷新按钮状态用）
    [[nodiscard]] bool IsRunning(TaskKind kind) const;

private:
    enum class TaskState {
        Stopped,
        Running,
    };

    // 单个任务的运行记录：三份数据同生共死，不存在下标错配
    struct Slot {
        std::jthread thread;
        TaskState state = TaskState::Stopped;
        unsigned generation = 0;
    };

    // 线程体：收 stop_token，返回本次任务的结果（产物 + 执行轮数）。
    // 内部数据必须在这里收尾完毕（Recorder 要 drain 光队列）再 return。
    // reason 不用自己算——launch 统一按 st.stop_requested() 覆盖，抛异常则记 Failed。
    using ThreadBody = std::function<TaskResult(std::stop_token)>;

    static constexpr std::size_t kTaskCount = 4;   // 与 TaskKind 的枚举值个数一致

    static_assert(static_cast<std::size_t>(TaskKind::Playback) + 1 == kTaskCount,
                  "When adding enum values to TaskKind, you need to update kTaskCount as well");

    [[nodiscard]] static constexpr std::size_t index(TaskKind kind) noexcept {
        return static_cast<std::size_t>(kind);
    }

    // 起线程的统一入口：冲突判定 → 防御性回收 → 推进序号 → 置 Running →
    // 包 try/catch → 结束通知（恰好一次）
    void launch(TaskKind kind, FinishCallback userCb, ThreadBody body);

    // StopXxx 的公共实现：request_stop + 阻塞 join + 置 Stopped
    void stopTask(TaskKind kind);

    // 仅 ~ThreadManager 调用：先全部叫停，再逐个回收
    void StopAll();

    std::shared_ptr<ITimer> &m_timerRef;   // 引用；只在 UI 线程读
    wxEvtHandler &m_ui;

    std::array<Slot, kTaskCount> m_slots{};
};
