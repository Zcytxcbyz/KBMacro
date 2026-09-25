#pragma once

#include <chrono>
#include <stop_token>

// ============================================================
// 可中断等待器
//
// 语义：
//   - wait() 期间收到停止请求 → 提前返回 false
//   - 正常等满                → 返回 true
//   - 无状态：同一实例可被多个线程并发调用（故 wait 为 const）
//
// 实现：SleepTimer（分段睡眠） / SpinTimer（忙等），见 common/Timer.hpp
//
// 生命周期：
//   MainFrame 持权威 shared_ptr<ITimer>，切换计时器模式时整体替换；
//   ThreadManager 持其引用，只在线程启动时快照一份进闭包。
//   因此"计时器模式切换只影响此后启动的任务，运行中的任务用它启动那一刻的
//   计时器跑完"。
//   线程侧约束（m_timerRef 只允许在 UI 线程读）见 managers/ThreadManager.hpp。
//
// 日志：
//   本层没有可记的事件。wait 属热路径，实现里禁止打日志；
//   唯一值得记的"模式切换"由 UI 侧负责。
// ============================================================

class ITimer {
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration  = std::chrono::milliseconds;

    ITimer() = default;
    virtual ~ITimer() = default;

    ITimer(const ITimer &) = delete;
    ITimer &operator=(const ITimer &) = delete;

    // 统一时间源。steady_clock 不受系统时间调整影响。
    // static：不持实例也能取时间；实例调用（t->now()）同样合法。
    [[nodiscard]] static TimePoint now() noexcept { return Clock::now(); }

    // 等待 d。true = 等满；false = 被 stop 请求打断。
    // d <= 0 视为"无需等待"，但仍先检查一次 stop。
    [[nodiscard]] virtual bool wait(Duration d, const std::stop_token &st) const = 0;
};
