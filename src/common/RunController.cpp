#include "common/RunController.hpp"

#include <chrono>

// ============================================================
// this file does not log:
//   - the constructor / noteIteration are called at task start and in every loop iteration;
//     the start log is written once by ThreadManager (only it knows the full parameters),
//     and the loop body is a hot path and is not logged by convention.
// ============================================================

RunController::RunController(const RunSpec &spec, const TimePoint start)
    : m_mode(spec.mode),
      m_start(start),
      m_duration(std::chrono::seconds(spec.durationSec)),
      m_totalCount(spec.count) {}

bool RunController::shouldRun(const TimePoint now) const {
    switch (m_mode) {
        case RunMode::Infinite:
            return true;

        case RunMode::Timed:
            // compare time points directly without going through milliseconds, matching the strict less-than rule.
            return now < m_start + m_duration;

        case RunMode::Counted:
            return m_executedCount < m_totalCount;
    }

    return false; // unreachable: all three RunMode values are covered
}

RunController::Duration RunController::clampWait(const Duration interval, const TimePoint now) const {
    if (m_mode != RunMode::Timed) return interval;

    // ceil instead of truncation: Duration is milliseconds, truncation would drop the sub-1ms remainder
    // and make "run N seconds" end early (same reason as in SleepTimer).
    const auto left = std::chrono::ceil<Duration>(m_start + m_duration - now);

    if (left <= Duration::zero()) return Duration::zero();
    return left < interval ? left : interval;
}

void RunController::noteIteration() {
    ++m_executedCount;
}
