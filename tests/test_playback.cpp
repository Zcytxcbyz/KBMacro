// ============================================================
// Playback module tests:
//   - round count vs action count, delayMs pre-wait, release stuck keys each round
//   - the start delay waits once up front and returns 0 at once if interrupted
//
// Three assumptions:
//   1. Timed rounds and clamping are not tested: ITimer::now() is static, so FakeTimer
//      cannot move the clock; RunController cases cover clampWait trimming.
//   2. the hold wait inside input::click also uses FakeTimer, so waited() mixes
//      holdMs (5ms) and the action gap (95ms); count them separately.
//   3. this target defines KBMACRO_DRY_RUN, so input events are only logged, not sent.
// ============================================================

#include <gtest/gtest.h>

#include <stop_token>
#include <vector>

#include "common/Action.hpp"
#include "common/Types.hpp"

#include "modules/Playback.hpp"

#include "FakeTimer.hpp"

namespace {

    constexpr ITimer::Duration kHold{5};     // one hold wait
    constexpr ITimer::Duration kDelay{95};   // gap between actions
    constexpr ITimer::Duration kStartDelay{300};

    RunSpec countedRun(const int count) {
        return RunSpec{.mode = RunMode::Counted, .count = count};
    }

    Action moveAction(const int delayMs) {
        return Action{.type = ActionType::MouseMove, .x = 10, .y = 20, .delayMs = delayMs};
    }

    // a click with a 5ms hold: input::click adds one extra 5ms wait
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

// ---- empty list and zero rounds ----

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

// ---- rounds and waits ----

TEST(PlaybackRun, CountedRepeatsScript) {
    // two actions, 2 rounds → 4 actions, 4 delay waits
    const std::vector<Action> actions{moveAction(95), moveAction(95)};
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, countedRun(2), timer, std::stop_token{}), 4);
    EXPECT_EQ(timer.countWaits(kDelay), 4u);
    EXPECT_EQ(timer.waitCalls(), 4);
}

TEST(PlaybackRun, ZeroDelaySkipsWait) {
    // delayMs == 0 produces no wait (wait(0) is meaningless)
    const std::vector<Action> actions{moveAction(0), moveAction(0), moveAction(0)};
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, countedRun(1), timer, std::stop_token{}), 3);
    EXPECT_EQ(timer.waitCalls(), 0);
}

TEST(PlaybackRun, ClickHoldWaitIsDistinctFromDelay) {
    const std::vector<Action> actions{clickAction(95)};
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, countedRun(2), timer, std::stop_token{}), 2);
    EXPECT_EQ(timer.countWaits(kHold), 2u);    // one hold wait per round
    EXPECT_EQ(timer.countWaits(kDelay), 2u);   // one pre-action gap per round
    EXPECT_EQ(timer.waitCalls(), 4);
}

TEST(PlaybackRun, KeyboardPairDoesNotLeak) {
    // Down has delay 0, Up has delay 95 → only one wait
    const std::vector<Action> actions{keyAction(KeyEventType::Down, 0),
                                      keyAction(KeyEventType::Up, 95)};
    FakeTimer timer;

    EXPECT_EQ(playback::run(actions, countedRun(1), timer, std::stop_token{}), 2);
    EXPECT_EQ(timer.countWaits(kDelay), 1u);
    EXPECT_EQ(timer.waitCalls(), 1);
}

// ---- interruption ----

TEST(PlaybackRun, StopDuringDelay) {
    const std::vector<Action> actions{moveAction(95)};
    FakeTimer timer(1);   // wait 1 already returns false

    EXPECT_EQ(playback::run(actions, RunSpec{.mode = RunMode::Infinite}, timer,
                            std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 1);
}

TEST(PlaybackRun, ClickInterruptedInsideHold) {
    // wait 1 is the pre-delay (success), wait 2 is the click hold wait (interrupted)
    const std::vector<Action> actions{clickAction(95)};
    FakeTimer timer(2);

    EXPECT_EQ(playback::run(actions, RunSpec{.mode = RunMode::Infinite}, timer,
                            std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 2);
}

// ---- start delay ----

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

// ---- drag: MouseDown / MouseUp are independent atomic actions ----

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
    // 3 rounds × 2 actions
    EXPECT_EQ(playback::run(actions, countedRun(3), timer, std::stop_token{}), 6);
    EXPECT_EQ(timer.countWaits(kDelay), 3u);
}

TEST(PlaybackRun, DelayActsAsPureWait) {
    const std::vector<Action> script{
        Action{.type = ActionType::Delay, .delayMs = 200},
    };

    FakeTimer timer;
    EXPECT_EQ(playback::run(script, countedRun(1), timer, std::stop_token{}), 1);

    // a delay action produces one wait and no input events
    ASSERT_EQ(timer.waited().size(), 1u);
    EXPECT_EQ(timer.waited()[0], ITimer::Duration{200});
}

TEST(PlaybackRun, RelativeClickKeepsHoldWait) {
    const std::vector<Action> script{
        Action{.type = ActionType::MouseClick,
               .x = 30,
               .y = -12,
               .relative = true,
               .button = MouseButton::Left,
               .holdMs = 40,
               .delayMs = 0},
    };

    FakeTimer timer;
    EXPECT_EQ(playback::run(script, countedRun(1), timer, std::stop_token{}), 1);

    // relative click = relative offset + hold wait; waited() holds only that one holdMs
    ASSERT_EQ(timer.waited().size(), 1u);
    EXPECT_EQ(timer.waited()[0], ITimer::Duration{40});
}
