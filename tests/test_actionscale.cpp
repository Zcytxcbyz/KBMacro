#include <gtest/gtest.h>

#include <vector>

#include "common/Action.hpp"
#include "common/ActionScale.hpp"

namespace {

    [[nodiscard]] Action makeClick(const int holdMs, const int delayMs) {
        Action a;
        a.type = ActionType::MouseClick;
        a.x = 400;
        a.y = 300;
        a.button = MouseButton::Left;
        a.holdMs = holdMs;
        a.delayMs = delayMs;
        return a;
    }

} // namespace

TEST(ActionScale, UnityFactorLeavesActionsUntouched) {
    std::vector<Action> actions{makeClick(50, 100)};

    action::scaleActions(actions, 1.0);

    EXPECT_EQ(actions[0].holdMs, 50);
    EXPECT_EQ(actions[0].delayMs, 100);
}

TEST(ActionScale, DoubleSpeedHalvesTimes) {
    std::vector<Action> actions{makeClick(50, 100)};

    action::scaleActions(actions, 2.0);

    EXPECT_EQ(actions[0].holdMs, 25);
    EXPECT_EQ(actions[0].delayMs, 50);
}

TEST(ActionScale, HalfSpeedDoublesTimes) {
    std::vector<Action> actions{makeClick(50, 100)};

    action::scaleActions(actions, 0.5);

    EXPECT_EQ(actions[0].holdMs, 100);
    EXPECT_EQ(actions[0].delayMs, 200);
}

TEST(ActionScale, DelayActionScalesItsDuration) {
    Action delay;
    delay.type = ActionType::Delay;
    delay.delayMs = 1000;
    std::vector<Action> actions{delay};

    action::scaleActions(actions, 2.0);

    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, ActionType::Delay);
    EXPECT_EQ(actions[0].delayMs, 500);
}

TEST(ActionScale, HoldIsClampedToMinimum) {
    // 4 倍速下 8ms 会被压到 2ms，必须夹到下限
    std::vector<Action> actions{makeClick(8, 100)};

    action::scaleActions(actions, 4.0);

    EXPECT_EQ(actions[0].holdMs, action::kMinHoldMs);
    EXPECT_EQ(actions[0].delayMs, 25);
}

TEST(ActionScale, CoordinatesAndKeysAreUntouched) {
    Action click = makeClick(50, 100);
    click.x = 1234;
    click.y = -56;
    click.relative = true;

    Action key;
    key.type = ActionType::Keyboard;
    key.hotkey = Hotkey{.modifiers = hk::kModCtrl, .vk = 0x41};
    key.keyEvent = KeyEventType::Down;
    key.delayMs = 30;

    std::vector<Action> actions{click, key};

    action::scaleActions(actions, 2.0);

    EXPECT_EQ(actions[0].x, 1234);
    EXPECT_EQ(actions[0].y, -56);
    EXPECT_TRUE(actions[0].relative);
    EXPECT_EQ(actions[1].hotkey.vk, 0x41u);
    EXPECT_EQ(actions[1].hotkey.modifiers, hk::kModCtrl);
    EXPECT_EQ(actions[1].keyEvent, KeyEventType::Down);
    EXPECT_EQ(actions[1].delayMs, 15);
    EXPECT_EQ(actions[1].holdMs, 0);   // 键盘动作没有按住时长，不该被写
}

TEST(ActionScale, NonPositiveFactorIsIgnored) {
    std::vector<Action> actions{makeClick(50, 100)};

    action::scaleActions(actions, 0.0);
    action::scaleActions(actions, -2.0);

    EXPECT_EQ(actions[0].holdMs, 50);
    EXPECT_EQ(actions[0].delayMs, 100);
}
