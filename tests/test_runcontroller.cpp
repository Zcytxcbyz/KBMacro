// ============================================================
// RunController 测试
//
// 纯逻辑：用合成时间点（不读真实时钟），因此完全确定性、微秒级跑完。
// 被测的是"运行模式"的判定，不涉及 ITimer 实例、不涉及线程。
// ============================================================

#include <gtest/gtest.h>

#include "common/RunController.hpp"

namespace {

    using Duration = ITimer::Duration;
    using TimePoint = ITimer::TimePoint;

    // 任意固定基准时刻。RunController 不该依赖真实时钟，
    // 用 Zero 作为基准就足够，且断言可以写成整数。
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const TimePoint kBase{};

    // kBase + ms
    TimePoint at(const long long ms) { return kBase + Duration(ms); }

} // namespace

// ============================================================
// Infinite
// ============================================================

TEST(RunControllerInfinite, AlwaysRuns) {
    const RunController rc(RunSpec{.mode = RunMode::Infinite}, kBase);

    EXPECT_TRUE(rc.shouldRun(kBase));
    EXPECT_TRUE(rc.shouldRun(at(3'600'000)));      // 一小时后
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
    EXPECT_TRUE(rc.shouldRun(at(9'999)));       // 差 1ms
    EXPECT_FALSE(rc.shouldRun(at(10'000)));     // 到点即 false（严格小于）
    EXPECT_FALSE(rc.shouldRun(at(10'001)));
}

TEST(RunControllerTimed, IgnoresCount) {
    // count = 0，但 Timed 模式应该完全无视它
    const RunController rc(
        RunSpec{.mode = RunMode::Timed, .durationSec = 10, .count = 0}, kBase);

    EXPECT_TRUE(rc.shouldRun(at(5'000)));
}

TEST(RunControllerTimed, ClampWaitShrinksToTimeLeft) {
    const RunController rc(RunSpec{.mode = RunMode::Timed, .durationSec = 10}, kBase);

    // 剩余 3s、间隔 10s → 只等 3s
    EXPECT_EQ(rc.clampWait(Duration(10'000), at(7'000)).count(), 3'000);

    // 剩余 12s、间隔 10s → 等满 10s
    EXPECT_EQ(rc.clampWait(Duration(10'000), at(-2'000)).count(), 10'000);

    // 已到 / 已过截止点 → 0
    EXPECT_EQ(rc.clampWait(Duration(10'000), at(10'000)).count(), 0);
    EXPECT_EQ(rc.clampWait(Duration(10'000), at(11'000)).count(), 0);
}

// 行为级：把循环骨架跑一遍，验证"长间隔不会把定时任务拖超时"
TEST(RunControllerTimed, LongIntervalDoesNotOverrun) {
    RunController rc(RunSpec{.mode = RunMode::Timed, .durationSec = 10}, kBase);

    TimePoint now = kBase;
    int iterations = 0;
    while (rc.shouldRun(now)) {
        ++iterations;
        rc.noteIteration();
        now += rc.clampWait(Duration(60'000), now);   // 模拟 ITimer::wait 等满
    }

    EXPECT_EQ(iterations, 1);
    EXPECT_EQ(now, at(10'000));       // 正好在截止点收工，而不是 60s
}

// ============================================================
// Counted
// ============================================================

TEST(RunControllerCounted, StopsAfterCount) {
    RunController rc(RunSpec{.mode = RunMode::Counted, .count = 3}, kBase);

    EXPECT_TRUE(rc.shouldRun(kBase));
    rc.noteIteration();                                // 1

    EXPECT_TRUE(rc.shouldRun(kBase));
    rc.noteIteration();                                // 2

    EXPECT_TRUE(rc.shouldRun(kBase));
    rc.noteIteration();                                // 3

    EXPECT_FALSE(rc.shouldRun(kBase));
    EXPECT_FALSE(rc.shouldRun(at(1'000'000)));         // 计满了就不再放开，与时间无关
}

TEST(RunControllerCounted, SingleIteration) {
    RunController rc(RunSpec{.mode = RunMode::Counted, .count = 1}, kBase);

    EXPECT_TRUE(rc.shouldRun(kBase));
    rc.noteIteration();
    EXPECT_FALSE(rc.shouldRun(kBase));
}

TEST(RunControllerCounted, ZeroCountRunsNothing) {
    // 钉死语义：count <= 0 就是零轮，不需要额外防御。
    // （T1 的 UI 限了 1..999999，但导入的 JSON 可能带 0。）
    const RunController rc(RunSpec{.mode = RunMode::Counted, .count = 0}, kBase);

    EXPECT_FALSE(rc.shouldRun(kBase));
}

TEST(RunControllerCounted, IgnoresDuration) {
    // durationSec = 0，但 Counted 模式应该完全无视它
    RunController rc(
        RunSpec{.mode = RunMode::Counted, .durationSec = 0, .count = 2}, kBase);

    EXPECT_TRUE(rc.shouldRun(at(3'600'000)));   // 一小时过去也照样跑
    rc.noteIteration();

    EXPECT_TRUE(rc.shouldRun(at(7'200'000)));
    rc.noteIteration();

    EXPECT_FALSE(rc.shouldRun(at(7'200'000)));
}

TEST(RunControllerCounted, ClampWaitReturnsInterval) {
    const RunController rc(RunSpec{.mode = RunMode::Counted, .count = 5}, kBase);

    EXPECT_EQ(rc.clampWait(Duration(60'000), at(999'999)).count(), 60'000);
}

// 行为级：循环骨架跑固定次数，不多不少
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
    EXPECT_EQ(now, at(400));     // 4 轮 × 100ms
}
