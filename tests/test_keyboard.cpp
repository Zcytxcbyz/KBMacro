// ============================================================
// Keyboard module tests:
//   - round counting, waits between rounds, interrupted by stop
//   - an empty hotkey is not rejected (module decision: validity belongs to the panel)
//
// Like the clicker: the hold wait inside input::pressKey also uses this FakeTimer,
// so the 5ms hold and the 95ms round gap must be counted separately.
// Timed round counts are not tested (ITimer::now() is static, the clock cannot be moved).
// ============================================================

#include <gtest/gtest.h>

#include "common/KeyboardParams.hpp"

#include "modules/Keyboard.hpp"

#include "FakeTimer.hpp"

namespace {

    // intervalMs = 100, holdMs = 5 → the round gap is 95, unlike the 5ms hold
    KeyboardParams makeParams(const RunMode mode, const int count,
                              const Hotkey &hotkey = Hotkey{.vk = 0x41}) {   // default A
        KeyboardParams p;
        p.hotkey = hotkey;
        p.intervalMs = 100;
        p.holdMs = 5;
        p.run = RunSpec{.mode = mode, .count = count};
        return p;
    }

    constexpr ITimer::Duration kHold{5};    // one hold wait
    constexpr ITimer::Duration kGap{95};    // round gap (100 - 5)

} // namespace

TEST(KeyboardRun, CountedRunsExactRounds) {
    FakeTimer timer;
    const KeyboardParams p = makeParams(RunMode::Counted, 3);

    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 3);

    // three hold waits + two round gaps (no wait after the last round)
    EXPECT_EQ(timer.countWaits(kHold), 3u);
    EXPECT_EQ(timer.countWaits(kGap), 2u);
    EXPECT_EQ(timer.waitCalls(), 5);
}

TEST(KeyboardRun, CountedZeroDoesNothing) {
    FakeTimer timer;
    const KeyboardParams p = makeParams(RunMode::Counted, 0);

    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 0);
}

TEST(KeyboardRun, GapDeductsHoldTime) {
    FakeTimer timer;
    const KeyboardParams p = makeParams(RunMode::Counted, 2);

    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 2);

    EXPECT_EQ(timer.countWaits(kGap), 1u);
    EXPECT_EQ(timer.countWaits(ITimer::Duration{100}), 0u);
}

TEST(KeyboardRun, StopOnFirstWaitCountsFirstRound) {
    FakeTimer timer(1);   // interrupted on the first wait (the hold of round 1)
    const KeyboardParams p = makeParams(RunMode::Infinite, 0);

    // the key was already pressed and released, so it counts as one round
    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 1);
    EXPECT_EQ(timer.waitCalls(), 1);
}

TEST(KeyboardRun, InfiniteStopsOnThirdRound) {
    // call sequence: hold, gap, hold, gap, hold, gap, ...
    // the 6th wait = the gap after round 3
    FakeTimer timer(6);
    const KeyboardParams p = makeParams(RunMode::Infinite, 0);

    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 3);
    EXPECT_EQ(timer.countWaits(kHold), 3u);
}

TEST(KeyboardRun, EmptyHotkeyIsNotRejected) {
    FakeTimer timer;
    const KeyboardParams p = makeParams(RunMode::Counted, 1, Hotkey{});   // vk=0, no modifiers

    // no validation here: an empty hotkey still runs one round, empty event table, one skipped log in input
    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 1);
    EXPECT_EQ(timer.countWaits(kHold), 1u);
}

// ---- start delay ----

TEST(KeyboardRun, StartDelayHappensBeforeFirstAction) {
    KeyboardParams p;
    p.hotkey = Hotkey{.modifiers = 0, .vk = 65};   // 'A'
    p.intervalMs = 100;
    p.holdMs = 5;
    p.run = RunSpec{.mode = RunMode::Counted, .count = 2};
    p.run.startDelayMs = 300;

    FakeTimer timer;
    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 2);

    const auto &w = timer.waited();
    ASSERT_EQ(w.size(), 4u);   // 1 delay + 2 holds + 1 round gap
    EXPECT_EQ(w[0], ITimer::Duration{300});
    EXPECT_EQ(timer.countWaits(ITimer::Duration{300}), 1u);
    EXPECT_EQ(timer.countWaits(ITimer::Duration{5}), 2u);
    EXPECT_EQ(timer.countWaits(ITimer::Duration{95}), 1u);
}

TEST(KeyboardRun, StartDelayInterruptedByStop) {
    KeyboardParams p;
    p.hotkey = Hotkey{.modifiers = 0, .vk = 65};
    p.intervalMs = 100;
    p.holdMs = 5;
    p.run = RunSpec{.mode = RunMode::Infinite, .count = 0};
    p.run.startDelayMs = 500;

    FakeTimer timer(1);
    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 1);
}
