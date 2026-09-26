#include "common/RunController.hpp"

#include <chrono>

// ============================================================
// 本文件不打日志：
//   - 构造 / noteIteration 的调用点在任务启动与每轮循环里，
//     启动日志由 ThreadManager 统一记一次（那里才知道完整参数），
//     循环体是热路径，按约定不记。
// ============================================================

RunController::RunController(const RunSpec &spec, const TimePoint start)
    : m_mode(spec.mode),
      m_start(start),
      m_duration(std::chrono::seconds(spec.durationSec)),
      m_totalCount(spec.count) {
}

bool RunController::shouldRun(const TimePoint now) const {
    switch (m_mode) {
        case RunMode::Infinite:
            return true;

        case RunMode::Timed:
            // 直接比时间点，不经过毫秒转换，保证与"严格小于"的口径一致。
            return now < m_start + m_duration;

        case RunMode::Counted:
            return m_executedCount < m_totalCount;
    }

    return false;   // 不可达：RunMode 三个值都已覆盖
}

RunController::Duration RunController::clampWait(const Duration interval,
                                                 const TimePoint now) const {
    if (m_mode != RunMode::Timed) return interval;

    // ceil 而不是截断：Duration 是毫秒，截断会把最后不到 1ms 的零头算没，
    // 让"定时 N 秒"提前结束（跟 SleepTimer 里同一个理由）。
    const auto left = std::chrono::ceil<Duration>(m_start + m_duration - now);

    if (left <= Duration::zero()) return Duration::zero();
    return left < interval ? left : interval;
}

void RunController::noteIteration() {
    ++m_executedCount;
}
