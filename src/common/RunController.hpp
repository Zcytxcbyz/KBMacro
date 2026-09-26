#pragma once

#include "common/ITimer.hpp"
#include "common/Types.hpp"

// ============================================================
// 运行模式判定器
//
// 纯逻辑：不持时间源、不 sleep、不碰 stop_token、不碰 UI/线程。
//   - "停止"  由 ITimer::wait 的返回值决定，与本类正交
//   - "现在"  由调用方传入（ITimer::now()），便于测试注入时间点
//
// 每个工作线程独占一个，在调用内部构造（不进线程函数参数）。
//
// 用法（三模块共用的循环骨架）：
//   RunController rc(p.run, ITimer::now());
//   while (rc.shouldRun(ITimer::now())) {
//       doOneAction();
//       rc.noteIteration();
//       if (!timer->wait(rc.clampWait(Duration(p.intervalMs), ITimer::now()), st))
//           break;                    // 被 stop：跳出
//   }
// ============================================================

class RunController {
public:
    using TimePoint = ITimer::TimePoint;
    using Duration = ITimer::Duration;

    // start 由调用方传（通常是线程启动时的 ITimer::now()）。
    // 定时模式的截止点固定为 start + durationSec，不做累计计时。
    RunController(const RunSpec &spec, TimePoint start);

    // 还能不能再跑一轮？
    //   Infinite: 恒 true
    //   Timed   : now < start + durationSec（严格小于）
    //   Counted : 已执行轮数 < count
    [[nodiscard]] bool shouldRun(TimePoint now) const;

    // 本轮实际该等多久？
    //   Infinite / Counted: 原样返回 interval
    //   Timed             : min(interval, 距截止点的剩余)，已过截止点返回 0
    //
    // 为什么要裁剪：间隔 60s + 定时 10s 时，若老实等满 60s，
    // 任务要跑 60s 才结束——"定时 10 秒"就成了假话。
    [[nodiscard]] Duration clampWait(Duration interval, TimePoint now) const;

    // 一轮完成，记账（Counted 用它递增）
    void noteIteration();

private:
    RunMode m_mode = RunMode::Infinite;
    TimePoint m_start{};
    Duration m_duration{};      // Timed：总时长
    int m_totalCount = 0;       // Counted：目标轮数
    int m_executedCount = 0;    // Counted：已完成轮数
};
