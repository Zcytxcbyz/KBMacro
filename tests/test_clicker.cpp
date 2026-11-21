// ============================================================
// Clicker module tests:
//   - toClickStep: a pure function, covers all ten ClickType values
//   - run: round counting, waits between rounds, interrupted by stop
//
// Two assumptions:
//   1. Timed round counts are not tested: ITimer::now() is static, so FakeTimer
//      cannot move the clock; RunController cases cover the timed decision.
//   2. the hold wait inside input::click also uses FakeTimer, so waited() mixes
//      holdMs (5ms) and the round gap (95ms); count them separately.
// This target defines KBMACRO_DRY_RUN, so input events are only logged, not sent.
// ============================================================

#include <gtest/gtest.h>

#include "common/ClickerParams.hpp"

#include "modules/Clicker.hpp"

#include "FakeTimer.hpp"

namespace {

// ignorePosition = true: skip moveTo each round, the test only checks click and wait.
// intervalMs = 100, holdMs = 5 -> button rounds wait 95ms, unlike the 5ms hold,
// so countWaits can tell the two kinds of waits apart.
ClickerParams makeParams(const RunMode mode, const int count, const ClickType type = ClickType::Left) {
    ClickerParams p;
    p.ignorePosition = true;
    p.clickType = type;
    p.intervalMs = 100;
    p.holdMs = 5;
    p.wheelSteps = 1;
    p.run = RunSpec{.mode = mode, .count = count};
    return p;
}

constexpr ITimer::Duration kHold{5}; // one hold wait
constexpr ITimer::Duration kGap{95}; // button round gap (100 - 5)

} // namespace

// ---- toClickStep ----

TEST(ToClickStep, ButtonsMapToButtons) {
    EXPECT_EQ(clicker::toClickStep(ClickType::Left).button, MouseButton::Left);
    EXPECT_EQ(clicker::toClickStep(ClickType::Right).button, MouseButton::Right);
    EXPECT_EQ(clicker::toClickStep(ClickType::Middle).button, MouseButton::Middle);
    EXPECT_EQ(clicker::toClickStep(ClickType::Side1).button, MouseButton::Side1);
    EXPECT_EQ(clicker::toClickStep(ClickType::Side2).button, MouseButton::Side2);

    for (const auto type : {ClickType::Left, ClickType::Right, ClickType::Middle, ClickType::Side1, ClickType::Side2}) {
        EXPECT_FALSE(clicker::toClickStep(type).wheel) << "type=" << clickTypeToString(type);
        EXPECT_EQ(clicker::toClickStep(type).times, 1) << "type=" << clickTypeToString(type);
    }
}

TEST(ToClickStep, WheelsMapToWheel) {
    EXPECT_TRUE(clicker::toClickStep(ClickType::WheelUp).wheel);
    EXPECT_TRUE(clicker::toClickStep(ClickType::WheelDown).wheel);
    EXPECT_TRUE(clicker::toClickStep(ClickType::WheelLeft).wheel);
    EXPECT_TRUE(clicker::toClickStep(ClickType::WheelRight).wheel);

    EXPECT_EQ(clicker::toClickStep(ClickType::WheelUp).direction, WheelDirection::Up);
    EXPECT_EQ(clicker::toClickStep(ClickType::WheelDown).direction, WheelDirection::Down);
    EXPECT_EQ(clicker::toClickStep(ClickType::WheelLeft).direction, WheelDirection::Left);
    EXPECT_EQ(clicker::toClickStep(ClickType::WheelRight).direction, WheelDirection::Right);

    for (const auto type : {ClickType::WheelUp, ClickType::WheelDown, ClickType::WheelLeft, ClickType::WheelRight}) {
        EXPECT_EQ(clicker::toClickStep(type).times, 1) << "type=" << clickTypeToString(type);
    }
}

TEST(ToClickStep, DoubleIsTwoLeftClicks) {
    const auto step = clicker::toClickStep(ClickType::Double);
    EXPECT_EQ(step.times, 2);
    EXPECT_EQ(step.button, MouseButton::Left);
    EXPECT_FALSE(step.wheel);
}

// ---- run ----

TEST(Run, CountedRunsExactRounds) {
    FakeTimer timer;
    const ClickerParams p = makeParams(RunMode::Counted, 3);

    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 3);

    // three rounds: three hold waits + two round gaps (no wait after the last round)
    EXPECT_EQ(timer.countWaits(kHold), 3u);
    EXPECT_EQ(timer.countWaits(kGap), 2u);
    EXPECT_EQ(timer.waitCalls(), 5);
}

TEST(Run, CountedZeroDoesNothing) {
    FakeTimer timer;
    const ClickerParams p = makeParams(RunMode::Counted, 0);

    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 0);
}

TEST(Run, DoubleCountsAsOneRound) {
    FakeTimer timer;
    const ClickerParams p = makeParams(RunMode::Counted, 2, ClickType::Double);

    // a double click counts as one round (same as Counted); two rounds = four holds, one gap
    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 2);
    EXPECT_EQ(timer.countWaits(kHold), 4u);
    EXPECT_EQ(timer.countWaits(kGap), 1u);
}

TEST(Run, GapDeductsHoldTime) {
    FakeTimer timer;
    const ClickerParams p = makeParams(RunMode::Counted, 2);

    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 2);

    // intervalMs = 100, holdMs = 5 -> the round gap is 95, not 100
    EXPECT_EQ(timer.countWaits(kGap), 1u);
    EXPECT_EQ(timer.countWaits(ITimer::Duration{100}), 0u);
}

TEST(Run, WheelUsesFullInterval) {
    FakeTimer timer;
    const ClickerParams p = makeParams(RunMode::Counted, 2, ClickType::WheelUp);

    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 2);

    // wheels have no hold, so the wait is the full interval; this is the only wait here
    EXPECT_EQ(timer.waitCalls(), 1);
    EXPECT_EQ(timer.countWaits(ITimer::Duration{100}), 1u);
}

TEST(Run, StopOnFirstWaitCountsFirstRound) {
    FakeTimer timer(1); // interrupted on the first wait (the hold of round 1)
    const ClickerParams p = makeParams(RunMode::Infinite, 0);

    // the press was already sent, so it counts as one round
    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 1);
    EXPECT_EQ(timer.waitCalls(), 1);
}

TEST(Run, InfiniteStopsOnThirdRound) {
    // call sequence: hold, gap, hold, gap, hold, gap, ...
    // the 6th wait = the gap after round 3 -> stopping there leaves exactly 3 rounds
    FakeTimer timer(6);
    const ClickerParams p = makeParams(RunMode::Infinite, 0);

    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 3);
    EXPECT_EQ(timer.countWaits(kHold), 3u);
}

// ---- start delay ----

TEST(Run, StartDelayHappensBeforeFirstAction) {
    ClickerParams p = makeParams(RunMode::Counted, 2);
    p.run.startDelayMs = 300;

    FakeTimer timer;
    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 2);

    // the first wait is the start delay; later holds/gaps are unaffected
    const auto &w = timer.waited();
    ASSERT_EQ(w.size(), 4u); // 1 delay + 2 holds + 1 round gap
    EXPECT_EQ(w[0], ITimer::Duration{300});
    EXPECT_EQ(timer.countWaits(ITimer::Duration{300}), 1u);
    EXPECT_EQ(timer.countWaits(kHold), 2u);
    EXPECT_EQ(timer.countWaits(kGap), 1u);
}

TEST(Run, StartDelayInterruptedByStop) {
    ClickerParams p = makeParams(RunMode::Infinite, 0);
    p.run.startDelayMs = 500;

    FakeTimer timer(1); // wait 1 is the start delay, interrupted right away
    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 1);
    ASSERT_EQ(timer.waited().size(), 1u);
    EXPECT_EQ(timer.waited()[0], ITimer::Duration{500});
}
