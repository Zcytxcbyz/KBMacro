#pragma once

#include <chrono>
#include <stop_token>

// ============================================================
// interruptible waiter
//
// semantics:
//   - stop requested during wait() -> returns false early
//   - waited the full duration    -> returns true
//   - stateless: one instance may be called concurrently by several threads (hence const wait)
//
// implementation: SleepTimer (chunked sleep) / SpinTimer (busy wait), see common/Timer.hpp
//
// lifetime:
//   MainFrame owns the authoritative shared_ptr<ITimer> and swaps it wholesale on a timer mode switch;
//   ThreadManager holds a reference and snapshots a copy into the closure only at thread start.
//   so "a timer mode switch only affects tasks started afterwards; a running task finishes with
//   the timer it captured at start".
//   thread-side constraints (m_timerRef may only be read on the UI thread) live in managers/ThreadManager.hpp.
//
// logging:
//   this layer has nothing worth logging. wait is a hot path, so implementations must not log;
//   the only loggable event, the "mode switch", is handled on the UI side.
// ============================================================

class ITimer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::milliseconds;

    ITimer() = default;
    virtual ~ITimer() = default;

    ITimer(const ITimer &) = delete;
    ITimer &operator=(const ITimer &) = delete;

    // single time source. steady_clock is immune to wall-clock adjustments.
    // static: usable without an instance; calling it on an instance (t->now()) is fine too.
    [[nodiscard]] static TimePoint now() noexcept { return Clock::now(); }

    // wait for d. true = waited fully; false = interrupted by a stop request.
    // d <= 0 means "no waiting needed", but stop is still checked once first.
    [[nodiscard]] virtual bool wait(Duration d, const std::stop_token &st) const = 0;
};
