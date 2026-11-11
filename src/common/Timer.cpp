#include "common/Timer.hpp"

#include <immintrin.h> // _mm_pause()
#include <thread> // std::this_thread::sleep_for

// ============================================================
// neither implementation logs: wait is a hot path and is not logged by convention.
// (the ENABLE_LOGGING switch does not matter here since there are no LOG_* calls at all.)
// ============================================================

// ============================================================
// SleepTimer: chunked sleep
// ============================================================

bool SleepTimer::wait(const Duration d, const std::stop_token &st) const {
    if (d <= Duration::zero()) return !st.stop_requested();

    // recompute the remainder from an absolute deadline instead of subtracting from it:
    // sleep_for always sleeps at least the requested time, so subtracting would accumulate
    // each overshoot into a total drift (sleeping later and later).
    const auto deadline = now() + d;

    while (true) {
        // check stop first: semantically "interrupted" wins over "just reached the deadline".
        if (st.stop_requested()) return false;
        if (now() >= deadline) return true;

        // ceil instead of duration_cast: Duration is milliseconds, plain truncation would drop
        // the sub-1ms remainder in the last round and return early (the lower-bound assert would fail).
        // ceil over-sleeps by less than 1ms, negligible against the system timer granularity.
        const auto remaining = std::chrono::ceil<Duration>(deadline - now());

        std::this_thread::sleep_for(remaining < kSlice ? remaining : kSlice);
    }
}

// ============================================================
// SpinTimer: busy wait
// ============================================================

bool SpinTimer::wait(const Duration d, const std::stop_token &st) const {
    if (d <= Duration::zero()) return !st.stop_requested();

    const auto deadline = now() + d;

    while (true) {
        if (st.stop_requested()) return false;
        if (now() >= deadline) return true;

        // spin hint: lower power draw and reduce execution-unit contention under SMT.
        // a plain while loop works without it but needlessly saturates the decode/execute ports.
        _mm_pause();
    }
}

// ============================================================
// HybridTimer: chunked sleep for long waits, busy wait at the tail
// ============================================================
// two phases:
//   1) remaining > spinTail → chunked sleep (absolute deadline recompute, same as SleepTimer)
//   2) remaining <= spinTail → busy-wait until the deadline
// the key is that phase one uses "remaining - spinTail", not "remaining": otherwise the
// overshoot of the last sleep chunk eats the tail and the hybrid is pointless.
bool HybridTimer::wait(const Duration d, const std::stop_token &st) const {
    if (d <= Duration::zero()) return !st.stop_requested();

    const auto deadline = now() + d;

    // ---- phase one: sleep until deadline - spinTail ----
    while (true) {
        if (st.stop_requested()) return false;

        const auto remaining = deadline - now();
        if (remaining <= Duration::zero()) return true;
        if (remaining <= m_spinTail) break; // enter the tail, hand over to the spin loop

        const auto sleepFor = std::chrono::ceil<Duration>(remaining - m_spinTail);
        std::this_thread::sleep_for(sleepFor < SleepTimer::kSlice ? sleepFor : SleepTimer::kSlice);
    }

    // ---- phase two: spin until the deadline ----
    while (true) {
        if (st.stop_requested()) return false;
        if (now() >= deadline) return true;

        _mm_pause();
    }
}
