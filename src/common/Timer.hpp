#pragma once

#include "common/ITimer.hpp"

// ============================================================
// the three ITimer implementations
//
//   SleepTimer — chunked sleep
//       CPU usage ≈ 0; stop-while-waiting latency = one slice aligned to a system tick
//       (nominal kSlice = 10ms, measured about 15–25ms);
//       but wait precision depends on the system timer granularity (Windows defaults to about 15.6ms),
//       so in the normal mode the real interval may be slightly longer than configured — a designed-in cost.
//
//   SpinTimer  — busy wait
//       best precision (bounded by steady_clock / QPC); stop response is sub-microsecond;
//       the cost is a full core while waiting. that is an explicit contract,
//       and it is why the UI shows a CPU warning dialog for "high-precision mode".
//
//   HybridTimer — hybrid
//       chunked sleep for long waits, busy wait at the tail; the threshold (tail spin) comes from config.
//       precision close to Spin, CPU burns only on the tail.
//
// all three are stateless: one instance may be called concurrently by several threads.
// implementation details (the deadline loop, why nothing is subtracted) are in Timer.cpp.
// ============================================================

// chunked sleep implementation.
//
// split a long wait into pieces of <= kSlice and check for a stop request between pieces.
// so the worst stop-while-waiting latency = one slice aligned to a system tick
// (nominal 10ms, measured about 15–25ms), which is not equal to the nominal kSlice.
// this latency is also the upper bound on the UI thread stall when Stop blocks in join in Sleep mode.
class SleepTimer final : public ITimer {
public:
    // slice granularity. exposed so tests can assert the upper bound of the stop latency.
    static constexpr Duration kSlice{10};

    [[nodiscard]] bool wait(Duration d, const std::stop_token &st) const override;
};

// busy-wait implementation.
//
// the loop checks for stop requests, separated by a single _mm_pause,
// so the response latency is sub-microsecond.
// note: CPU usage is 100% (one core) while waiting — a designed-in tradeoff, not a defect.
class SpinTimer final : public ITimer {
public:
    [[nodiscard]] bool wait(Duration d, const std::stop_token &st) const override;
};

// hybrid implementation: chunked sleep for long waits, busy wait at the tail.
//
// semantics (T = tail spin duration given at construction):
//   - d >  T: chunked-sleep until deadline - T, then busy-wait the last T
//   - d <= T: busy-wait the whole time (equivalent to SpinTimer then)
// precision: error = max(0, overshoot of the last sleep chunk - T); with T=10ms usually single-digit ms.
// stop response: upper bound about T (slice alignment in sleep chunks, sub-microsecond while spinning).
// cost: those T milliseconds of spinning occupy a full core — the same contract as SpinTimer.
// T <= 0 degrades to plain chunked sleep (defensive; the UI never supplies such a value).
class HybridTimer final : public ITimer {
public:
    explicit HybridTimer(const Duration spinTail) noexcept
        : m_spinTail(spinTail > Duration::zero() ? spinTail : Duration::zero()) {}

    [[nodiscard]] bool wait(Duration d, const std::stop_token &st) const override;

    [[nodiscard]] Duration spinTail() const noexcept { return m_spinTail; }

private:
    Duration m_spinTail;
};
