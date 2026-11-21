// ============================================================
// RunController tests
//
// Pure logic: synthetic time points (no real clock), so fully deterministic and instant.
// What is tested is the "run mode" decision; no ITimer instance, no threads.
// ============================================================

#include <gtest/gtest.h>

#include "common/RunController.hpp"

namespace {

using Duration = ITimer::Duration;
using TimePoint = ITimer::TimePoint;

// an arbitrary fixed base time. RunController must not depend on the real clock,
// so Zero is a sufficient base and assertions can be written as integers.
// ReSharper disable once CppVariableCanBeMadeConstexpr
const TimePoint kBase{};

// kBase + ms
TimePoint at(const long long ms) {
    return kBase + Duration(ms);
}

} // namespace

// ============================================================
// Infinite
// ============================================================

TEST(RunControllerInfinite, AlwaysRuns) {
    const RunController rc(RunSpec{.mode = RunMode::Infinite}, kBase);

    EXPECT_TRUE(rc.shouldRun(kBase));
    EXPECT_TRUE(rc.shouldRun(at(3'600'000))); // one hour later
}

TEST(RunControllerInfinite, NoteIterationDoesNotStop) {
    RunController rc(RunSpec{.mode = RunMode::Infinite}, kBase);

    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(rc.shouldRun(kBase));
        rc.noteIteration();
    }
}

TEST(RunControllerInfinite, ClampWaitReturnsInterval) {
    const RunController rc(RunSpec{.mode = RunMode::Infinite}, kBase);

    EXPECT_EQ(rc.clampWait(Duration(60'000), at(999'999)).count(), 60'000);
}

// ============================================================
// Timed
// ============================================================

TEST(RunControllerTimed, RunsUntilDeadline) {
    const RunController rc(RunSpec{.mode = RunMode::Timed, .durationSec = 10}, kBase);

    EXPECT_TRUE(rc.shouldRun(kBase));
    EXPECT_TRUE(rc.shouldRun(at(9'999))); // 1ms short
    EXPECT_FALSE(rc.shouldRun(at(10'000))); // at the deadline it is false (strictly less)
    EXPECT_FALSE(rc.shouldRun(at(10'001)));
}

TEST(RunControllerTimed, IgnoresCount) {
    // count = 0, but Timed mode must ignore it entirely
    const RunController rc(RunSpec{.mode = RunMode::Timed, .durationSec = 10, .count = 0}, kBase);

    EXPECT_TRUE(rc.shouldRun(at(5'000)));
}

TEST(RunControllerTimed, ClampWaitShrinksToTimeLeft) {
    const RunController rc(RunSpec{.mode = RunMode::Timed, .durationSec = 10}, kBase);

    // 3s left, 10s interval -> wait only 3s
    EXPECT_EQ(rc.clampWait(Duration(10'000), at(7'000)).count(), 3'000);

    // 12s left, 10s interval -> wait the full 10s
    EXPECT_EQ(rc.clampWait(Duration(10'000), at(-2'000)).count(), 10'000);

    // deadline reached / already past -> 0
    EXPECT_EQ(rc.clampWait(Duration(10'000), at(10'000)).count(), 0);
    EXPECT_EQ(rc.clampWait(Duration(10'000), at(11'000)).count(), 0);
}

// behavior level: run the loop skeleton once to show a long interval cannot push a Timed task past its deadline
TEST(RunControllerTimed, LongIntervalDoesNotOverrun) {
    RunController rc(RunSpec{.mode = RunMode::Timed, .durationSec = 10}, kBase);

    TimePoint now = kBase;
    int iterations = 0;
    while (rc.shouldRun(now)) {
        ++iterations;
        rc.noteIteration();
        now += rc.clampWait(Duration(60'000), now); // simulate ITimer::wait running to completion
    }

    EXPECT_EQ(iterations, 1);
    EXPECT_EQ(now, at(10'000)); // finishes exactly at the deadline, not after 60s
}

// ============================================================
// Counted
// ============================================================

TEST(RunControllerCounted, StopsAfterCount) {
    RunController rc(RunSpec{.mode = RunMode::Counted, .count = 3}, kBase);

    EXPECT_TRUE(rc.shouldRun(kBase));
    rc.noteIteration(); // 1

    EXPECT_TRUE(rc.shouldRun(kBase));
    rc.noteIteration(); // 2

    EXPECT_TRUE(rc.shouldRun(kBase));
    rc.noteIteration(); // 3

    EXPECT_FALSE(rc.shouldRun(kBase));
    EXPECT_FALSE(rc.shouldRun(at(1'000'000))); // once the count is used up it stops, regardless of time
}

TEST(RunControllerCounted, SingleIteration) {
    RunController rc(RunSpec{.mode = RunMode::Counted, .count = 1}, kBase);

    EXPECT_TRUE(rc.shouldRun(kBase));
    rc.noteIteration();
    EXPECT_FALSE(rc.shouldRun(kBase));
}

TEST(RunControllerCounted, ZeroCountRunsNothing) {
    // pin the semantics: count <= 0 means zero rounds, no extra guard needed.
    // (the T1 UI clamps to 1..999999, but an imported JSON may carry 0.)
    const RunController rc(RunSpec{.mode = RunMode::Counted, .count = 0}, kBase);

    EXPECT_FALSE(rc.shouldRun(kBase));
}

TEST(RunControllerCounted, IgnoresDuration) {
    // durationSec = 0, but Counted mode must ignore it entirely
    RunController rc(RunSpec{.mode = RunMode::Counted, .durationSec = 0, .count = 2}, kBase);

    EXPECT_TRUE(rc.shouldRun(at(3'600'000))); // runs as usual even after an hour
    rc.noteIteration();

    EXPECT_TRUE(rc.shouldRun(at(7'200'000)));
    rc.noteIteration();

    EXPECT_FALSE(rc.shouldRun(at(7'200'000)));
}

TEST(RunControllerCounted, ClampWaitReturnsInterval) {
    const RunController rc(RunSpec{.mode = RunMode::Counted, .count = 5}, kBase);

    EXPECT_EQ(rc.clampWait(Duration(60'000), at(999'999)).count(), 60'000);
}

// Behavioural: the loop skeleton runs the fixed count, no more, no less
TEST(RunControllerCounted, LoopRunsExactCount) {
    RunController rc(RunSpec{.mode = RunMode::Counted, .count = 4}, kBase);

    TimePoint now = kBase;
    int iterations = 0;
    while (rc.shouldRun(now)) {
        ++iterations;
        rc.noteIteration();
        now += rc.clampWait(Duration(100), now);
    }

    EXPECT_EQ(iterations, 4);
    EXPECT_EQ(now, at(400)); // 4 rounds x 100ms
}
