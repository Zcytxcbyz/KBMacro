// ============================================================
// 回放模块测试：
//   - 轮数与动作条数口径、delayMs 前置等待、每轮补平
//   - 开始延迟只在最前面等一次，被打断时直接返回 0
//
// 三个前提：
//   1. 不测 Timed 的轮数与截断：ITimer::now() 是静态的，FakeTimer 改不了时钟；
//      clampWait 的裁剪由 RunController 的用例覆盖。
//   2. input::click 内部的按住等待也走 FakeTimer，所以 waited() 里混着
//      holdMs（5ms）和动作间隔（95ms）两种时长，分开数。
//   3. 本目标已定义 KBMACRO_DRY_RUN，输入事件只记日志不真发。
// ============================================================

#include <gtest/gtest.h>

#include <stop_token>
#include <vector>

#include "common/Action.hpp"
#include "common/Types.hpp"

#include "modules/Playback.hpp"

#include "FakeTimer.hpp"

namespace {

    constexpr ITimer::Duration kHold{5};     // 一次按住的等待
    constexpr ITimer::Duration kDelay{95};   // 动作之间的间隔
    constexpr ITimer::Duration kStartDelay{300};

    RunSpec countedRun(const int count) {
        return RunSpec{.mode = RunMode::Counted, .count = count};
    }

    Action moveAction(const int delayMs) {
        return Action{.type = ActionType::MouseMove, .x = 10, .y = 20, .delayMs = delayMs};
    }

    // 按住 5ms 的点击：input::click 里会多出一次 5ms 等待
    Action clickAction(const int delayMs) {
        return Action{.type = ActionType::MouseClick,
                      .x = 0,
                      .y = 0,
                      .button = MouseButton::Left,
                      .holdMs = 5,
                      .delayMs = delayMs};
    }

    Action keyAction(const KeyEventType keyEvent, const int delayMs) {
        return Action{.type = ActionType::Keyboard,
                      .hotkey = Hotkey{.modifiers = 0, .vk = 65},   // 'A'
                      .keyEvent = keyEvent,
                      .delayMs = delayMs};
    }

} // namespace

// ---- 空列表与零轮 ----

TEST(PlaybackRun, EmptyListReturnsZero) {
    const std::vector<Action> actions;
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, countedRun(5), timer, std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 0);
}

TEST(PlaybackRun, CountedZeroDoesNothing) {
    const std::vector<Action> actions{moveAction(95)};
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, countedRun(0), timer, std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 0);
}

// ---- 轮数与等待 ----

TEST(PlaybackRun, CountedRepeatsScript) {
    // 两条动作，跑 2 轮 → 4 条、4 次延迟等待
    const std::vector<Action> actions{moveAction(95), moveAction(95)};
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, countedRun(2), timer, std::stop_token{}), 4);
    EXPECT_EQ(timer.countWaits(kDelay), 4u);
    EXPECT_EQ(timer.waitCalls(), 4);
}

TEST(PlaybackRun, ZeroDelaySkipsWait) {
    // delayMs == 0 不产生等待（wait(0) 没有意义）
    const std::vector<Action> actions{moveAction(0), moveAction(0), moveAction(0)};
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, countedRun(1), timer, std::stop_token{}), 3);
    EXPECT_EQ(timer.waitCalls(), 0);
}

TEST(PlaybackRun, ClickHoldWaitIsDistinctFromDelay) {
    const std::vector<Action> actions{clickAction(95)};
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, countedRun(2), timer, std::stop_token{}), 2);
    EXPECT_EQ(timer.countWaits(kHold), 2u);    // 每轮一次按住等待
    EXPECT_EQ(timer.countWaits(kDelay), 2u);   // 每轮一次前置间隔
    EXPECT_EQ(timer.waitCalls(), 4);
}

TEST(PlaybackRun, KeyboardPairDoesNotLeak) {
    // Down 的 delay 为 0，Up 的 delay 为 95 → 只有一次等待
    const std::vector<Action> actions{keyAction(KeyEventType::Down, 0),
                                      keyAction(KeyEventType::Up, 95)};
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, countedRun(1), timer, std::stop_token{}), 2);
    EXPECT_EQ(timer.countWaits(kDelay), 1u);
    EXPECT_EQ(timer.waitCalls(), 1);
}

// ---- 打断 ----

TEST(PlaybackRun, StopDuringDelay) {
    const std::vector<Action> actions{moveAction(95)};
    FakeTimer timer(1);   // 第 1 次 wait 就返回 false

    EXPECT_EQ(playback::run(actions, RunSpec{.mode = RunMode::Infinite}, timer,
                            std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 1);
}

TEST(PlaybackRun, ClickInterruptedInsideHold) {
    // 第 1 次 wait 是前置延迟（成功），第 2 次是 click 的按住等待（被打断）
    const std::vector<Action> actions{clickAction(95)};
    FakeTimer timer(2);

    EXPECT_EQ(playback::run(actions, RunSpec{.mode = RunMode::Infinite}, timer,
                            std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 2);
}

// ---- 开始延迟 ----

TEST(PlaybackRun, StartDelayBeforeFirstAction) {
    const std::vector<Action> actions{moveAction(95)};
    RunSpec run = countedRun(1);
    run.startDelayMs = static_cast<int>(kStartDelay.count());
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, run, timer, std::stop_token{}), 1);
    EXPECT_EQ(timer.countWaits(kStartDelay), 1u);
    EXPECT_EQ(timer.countWaits(kDelay), 1u);
    EXPECT_EQ(timer.waitCalls(), 2);
}

TEST(PlaybackRun, StartDelayInterruptedReturnsZero) {
    const std::vector<Action> actions{moveAction(95)};
    RunSpec run = countedRun(1);
    run.startDelayMs = static_cast<int>(kStartDelay.count());
    FakeTimer timer(1);

    EXPECT_EQ(playback::run(actions, run, timer, std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 1);
}

// ---- 拖动：MouseDown / MouseUp 是独立的原子动作 ----

TEST(Playback, MouseDownUpCountsTwoActions) {
    const std::vector<Action> actions{
        Action{.type = ActionType::MouseDown,
               .x = 100, .y = 200, .button = MouseButton::Left, .delayMs = 0},
        Action{.type = ActionType::MouseUp,
               .x = 300, .y = 400, .button = MouseButton::Left, .delayMs = kDelay.count()},
    };

    FakeTimer timer;
    EXPECT_EQ(playback::run(actions, countedRun(1), timer, std::stop_token{}), 2);
    EXPECT_EQ(timer.countWaits(kDelay), 1u);
}

TEST(Playback, MouseDownWithoutUpDoesNotHang) {
    const std::vector<Action> actions{
        Action{.type = ActionType::MouseDown,
               .x = 50, .y = 60, .button = MouseButton::Right, .delayMs = 0},
    };

    FakeTimer timer;
    EXPECT_EQ(playback::run(actions, countedRun(1), timer, std::stop_token{}), 1);
}

TEST(Playback, StuckMouseButtonIsReleasedEveryRound) {
    const std::vector<Action> actions{
        Action{.type = ActionType::MouseDown,
               .x = 50, .y = 60, .button = MouseButton::Left, .delayMs = 0},
        Action{.type = ActionType::MouseMove, .x = 80, .y = 90, .delayMs = kDelay.count()},
    };

    FakeTimer timer;
    // 3 轮 × 2 条动作
    EXPECT_EQ(playback::run(actions, countedRun(3), timer, std::stop_token{}), 6);
    EXPECT_EQ(timer.countWaits(kDelay), 3u);
}
