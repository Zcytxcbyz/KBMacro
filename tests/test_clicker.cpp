// ============================================================
// 连点器模块测试：
//   - toClickStep：纯函数，十个 ClickType 全覆盖
//   - run：轮数口径、轮间等待、被 stop 打断
//
// 两个前提：
//   1. 不测 Timed 的轮数：ITimer::now() 是静态的，FakeTimer 改不了时钟。
//      定时判定由 RunController 的用例覆盖。
//   2. input::click 内部的按住等待也走 FakeTimer，所以 waited() 里混着
//      holdMs（5ms）和轮间隔（95ms）两种时长，分开数。
// 本目标已定义 KBMACRO_DRY_RUN，输入事件只记日志不真发。
// ============================================================

#include <gtest/gtest.h>

#include "common/ClickerParams.hpp"

#include "modules/Clicker.hpp"

#include "FakeTimer.hpp"

namespace {

    // ignorePosition = true：跳过每轮 moveTo，测试只关心点击与等待。
    // intervalMs = 100、holdMs = 5 → 按钮类轮间等 95ms，与按住的 5ms 不同，
    // 这样 countWaits 才能把两种等待分开。
    ClickerParams makeParams(const RunMode mode, const int count,
                             const ClickType type = ClickType::Left) {
        ClickerParams p;
        p.ignorePosition = true;
        p.clickType = type;
        p.intervalMs = 100;
        p.holdMs = 5;
        p.wheelSteps = 1;
        p.run = RunSpec{.mode = mode, .count = count};
        return p;
    }

    constexpr ITimer::Duration kHold{5};    // 一次按住的等待
    constexpr ITimer::Duration kGap{95};    // 按钮类的轮间隔（100 - 5）

} // namespace

// ---- toClickStep ----

TEST(ToClickStep, ButtonsMapToButtons) {
    EXPECT_EQ(clicker::toClickStep(ClickType::Left).button, MouseButton::Left);
    EXPECT_EQ(clicker::toClickStep(ClickType::Right).button, MouseButton::Right);
    EXPECT_EQ(clicker::toClickStep(ClickType::Middle).button, MouseButton::Middle);
    EXPECT_EQ(clicker::toClickStep(ClickType::Side1).button, MouseButton::Side1);
    EXPECT_EQ(clicker::toClickStep(ClickType::Side2).button, MouseButton::Side2);

    for (const auto type : {ClickType::Left, ClickType::Right, ClickType::Middle, ClickType::Side1,
                            ClickType::Side2}) {
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

    for (const auto type : {ClickType::WheelUp, ClickType::WheelDown, ClickType::WheelLeft,
                            ClickType::WheelRight}) {
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

    // 三轮回合：三次按住等待 + 两次轮间隔（最后一轮之后不再等）
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

    // 双击一轮算一次（与 Counted 的次数口径一致）；两轮共四次按住等待、一次间隔
    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 2);
    EXPECT_EQ(timer.countWaits(kHold), 4u);
    EXPECT_EQ(timer.countWaits(kGap), 1u);
}

TEST(Run, GapDeductsHoldTime) {
    FakeTimer timer;
    const ClickerParams p = makeParams(RunMode::Counted, 2);

    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 2);

    // intervalMs = 100、holdMs = 5 → 轮间隔 95，不是 100
    EXPECT_EQ(timer.countWaits(kGap), 1u);
    EXPECT_EQ(timer.countWaits(ITimer::Duration{100}), 0u);
}

TEST(Run, WheelUsesFullInterval) {
    FakeTimer timer;
    const ClickerParams p = makeParams(RunMode::Counted, 2, ClickType::WheelUp);

    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 2);

    // 滚轮没有按住，等待就是完整间隔；整个测试里只有这一次等待
    EXPECT_EQ(timer.waitCalls(), 1);
    EXPECT_EQ(timer.countWaits(ITimer::Duration{100}), 1u);
}

TEST(Run, StopOnFirstWaitCountsFirstRound) {
    FakeTimer timer(1);   // 第一次等待（第 1 轮的按住）就"被打断"
    const ClickerParams p = makeParams(RunMode::Infinite, 0);

    // 按下已经发出去了，所以算一轮
    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 1);
    EXPECT_EQ(timer.waitCalls(), 1);
}

TEST(Run, InfiniteStopsOnThirdRound) {
    // 调用序列：hold, gap, hold, gap, hold, gap, ...
    // 第 6 次等待 = 第 3 轮之后的间隔 → 打断后正好跑满 3 轮
    FakeTimer timer(6);
    const ClickerParams p = makeParams(RunMode::Infinite, 0);

    EXPECT_EQ(clicker::run(p, timer, std::stop_token{}), 3);
    EXPECT_EQ(timer.countWaits(kHold), 3u);
}
