#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "common/ITimer.hpp"

// ============================================================
// Test timer: never sleeps, it only records calls and returns as scripted.
//
// Note 1: ITimer::now() is static, so FakeTimer cannot control time flow; it
//         only makes wait return at once. Timed-mode round counts need a real
//         wait for durationSec, so they are left to the RunController tests.
// Note 2: the "hold wait" inside input::click()/pressKey() also uses this
//         ITimer, so waited() mixes holdMs and round intervals. Count each
//         with countWaits(duration) instead of deriving rounds from the total.
// ============================================================
class FakeTimer final : public ITimer {
public:
    // stopAtCall: the Nth wait (1-based; holds and intervals both count) returns false.
    //             <= 0 means never stop.
    explicit FakeTimer(const int stopAtCall = 0) noexcept
        : m_stopAtCall(stopAtCall) {}

    [[nodiscard]] bool wait(const Duration d, const std::stop_token &st) const override {
        ++m_waitCalls;
        m_waited.push_back(d);

        if (st.stop_requested()) return false;
        if (m_stopAtCall > 0 && m_waitCalls >= m_stopAtCall) return false;
        return true;
    }

    [[nodiscard]] int waitCalls() const noexcept { return m_waitCalls; }

    [[nodiscard]] const std::vector<Duration> &waited() const noexcept { return m_waited; }

    // Number of waits with the given duration (the only way to tell holds from round intervals)
    [[nodiscard]] std::size_t countWaits(const Duration d) const {
        return static_cast<std::size_t>(std::count(m_waited.begin(), m_waited.end(), d));
    }

private:
    int m_stopAtCall = 0;
    mutable int m_waitCalls = 0;
    mutable std::vector<Duration> m_waited;
};
