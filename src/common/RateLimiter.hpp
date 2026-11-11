#pragma once

#include "common/ITimer.hpp"

// rate limiter: allows one call through per interval.
//
// purpose: throttle logs/reports of high-frequency events to a readable rate (one recording line per second).
// why a separate class: it is a generic helper used in many places; copying "record time + compare interval"
//   easily gets the edge cases wrong (allow immediately on first call, does equal to the interval count).
//
// contract:
//   - pure logic: no logging, no config reads, no threads; time comes from the caller, easy to inject in tests
//   - the first call is allowed immediately (no "blank first second")
//   - the test is "time since last allow >= interval", not a fixed tick (no accumulation, no compensation)
class RateLimiter final {
public:
    explicit RateLimiter(const ITimer::Duration interval) noexcept : m_interval(interval) {}

    // check against the current steady_clock (not a system call)
    [[nodiscard]] bool allow() noexcept { return allow(ITimer::now()); }

    // check against a given time point (test injection)
    [[nodiscard]] bool allow(const ITimer::TimePoint now) noexcept {
        if (!m_started) {
            m_started = true;
            m_last = now;
            return true;
        }
        if (now - m_last < m_interval) return false;
        m_last = now;
        return true;
    }

    void reset() noexcept { m_started = false; }

    [[nodiscard]] ITimer::Duration interval() const noexcept { return m_interval; }

private:
    ITimer::Duration m_interval{};
    ITimer::TimePoint m_last{};
    bool m_started = false;
};
