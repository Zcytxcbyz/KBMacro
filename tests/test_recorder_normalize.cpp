// ============================================================
// 归一化纯函数测试：
//   - 键盘：不带修饰键、自动重复合并、无 Down 的 Up 丢弃
//   - 鼠标：点击 vs 拖动（2px 阈值）、拖动端点坐标、多键同按
//   - 滚轮：累积成整格、残量保留
//   - 通用：头尾修饰键剔除、主键过滤、delayMs、动作上限
// 不测钩子与消息泵。
// ============================================================

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "common/Action.hpp"
#include "common/Types.hpp"

#include "modules/RecorderNormalize.hpp"

namespace {

    using recorder::NormalizeOptions;
    using recorder::RawEvent;
    using recorder::RawKind;

    // 测试文件不 include <windows.h>，这里直接写 VK 值
    constexpr unsigned kVkShift = 0x10;     // VK_SHIFT
    constexpr unsigned kVkControl = 0x11;   // VK_CONTROL

    NormalizeOptions plainOpts() {
        NormalizeOptions opts;
        return opts;
    }

    RawEvent keyEvent(const RawKind kind, const unsigned vk, const std::uint32_t t) {
        return RawEvent{.kind = kind, .vk = vk, .time = t};
    }

    RawEvent moveEvent(const int x, const int y, const std::uint32_t t) {
        return RawEvent{.kind = RawKind::Move, .x = x, .y = y, .time = t};
    }

    RawEvent buttonEvent(const RawKind kind, const MouseButton button, const int x, const int y,
                         const std::uint32_t t) {
        return RawEvent{.kind = kind, .x = x, .y = y, .button = button, .time = t};
    }

    RawEvent wheelEvent(const int delta, const std::uint32_t t, const bool horizontal = false) {
        return RawEvent{.kind = RawKind::Wheel,
                        .wheelDelta = delta,
                        .time = t,
                        .horizontal = horizontal};
    }

} // namespace

// ---- 空输入 ----

TEST(Normalize, EmptyInputYieldsNothing) {
    EXPECT_TRUE(recorder::normalize({}, plainOpts()).empty());
}

// ---- 键盘 ----

TEST(Normalize, KeyPairBecomesDownThenUp) {
    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, 65, 0),    // A
        keyEvent(RawKind::KeyUp, 65, 30),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 2u);

    EXPECT_EQ(actions[0].type, ActionType::Keyboard);
    EXPECT_EQ(actions[0].keyEvent, KeyEventType::Down);
    EXPECT_EQ(actions[0].hotkey.vk, 65u);
    EXPECT_EQ(actions[0].delayMs, 0);

    EXPECT_EQ(actions[1].keyEvent, KeyEventType::Up);
    EXPECT_EQ(actions[1].delayMs, 30);
}

TEST(Normalize, KeyboardActionsCarryNoModifiers) {
    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, 65, 0),
        keyEvent(RawKind::KeyUp, 65, 10),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 2u);

    // 和弦靠事件序列表达，单条动作的 modifiers 必须恒为 0
    EXPECT_EQ(actions[0].hotkey.modifiers, 0u);
    EXPECT_EQ(actions[1].hotkey.modifiers, 0u);
}

TEST(Normalize, AutoRepeatIsMerged) {
    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, 65, 0),
        keyEvent(RawKind::KeyDown, 65, 10),
        keyEvent(RawKind::KeyDown, 65, 20),
        keyEvent(RawKind::KeyUp, 65, 40),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[1].delayMs, 40);
}

TEST(Normalize, StartAndStopMainKeysAreDropped) {
    NormalizeOptions opts = plainOpts();
    opts.startKeyVk = 0x70;   // F1
    opts.stopKeyVk = 0x71;    // F2

    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, 0x70, 0),
        keyEvent(RawKind::KeyDown, 65, 5),
        keyEvent(RawKind::KeyUp, 65, 10),
        keyEvent(RawKind::KeyUp, 0x70, 15),
        keyEvent(RawKind::KeyDown, 0x71, 20),
    };

    const auto actions = recorder::normalize(events, opts);
    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0].hotkey.vk, 65u);
    EXPECT_EQ(actions[1].hotkey.vk, 65u);
}

TEST(Normalize, HeadModifierReleaseIsTrimmed) {
    NormalizeOptions opts = plainOpts();
    opts.startMods = hk::kModCtrl;

    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyUp, kVkControl, 0),
        keyEvent(RawKind::KeyDown, 65, 5),
        keyEvent(RawKind::KeyUp, 65, 10),
    };

    const auto actions = recorder::normalize(events, opts);
    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0].delayMs, 0);   // 剔除后重新归零
}

TEST(Normalize, TailModifierReleaseIsTrimmed) {
    NormalizeOptions opts = plainOpts();
    opts.stopMods = hk::kModShift;

    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, 65, 0),
        keyEvent(RawKind::KeyUp, 65, 10),
        keyEvent(RawKind::KeyUp, kVkShift, 20),
    };

    const auto actions = recorder::normalize(events, opts);
    ASSERT_EQ(actions.size(), 2u);
}

TEST(Normalize, TrimStopsAtFirstRealAction) {
    NormalizeOptions opts = plainOpts();
    opts.startMods = hk::kModCtrl;

    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyUp, kVkControl, 0),    // 头部修饰键，剔除
        keyEvent(RawKind::KeyDown, 65, 5),
        keyEvent(RawKind::KeyUp, 65, 10),
        keyEvent(RawKind::KeyDown, kVkControl, 15), // 用户真按的 Ctrl，保留
        keyEvent(RawKind::KeyUp, kVkControl, 20),
    };

    const auto actions = recorder::normalize(events, opts);
    ASSERT_EQ(actions.size(), 4u);
    EXPECT_EQ(actions[2].hotkey.vk, kVkControl);
}

// ---- 鼠标：点击 vs 拖动 ----

TEST(Normalize, ClickWithinThresholdBecomesMouseClick) {
    const std::vector<RawEvent> events{
        buttonEvent(RawKind::ButtonDown, MouseButton::Left, 100, 100, 0),
        buttonEvent(RawKind::ButtonUp, MouseButton::Left, 101, 101, 50),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, ActionType::MouseClick);
    EXPECT_EQ(actions[0].button, MouseButton::Left);
    EXPECT_EQ(actions[0].x, 100);
    EXPECT_EQ(actions[0].y, 100);
    EXPECT_EQ(actions[0].holdMs, 50);
}

TEST(Normalize, SmallJitterIsStillClick) {
    const std::vector<RawEvent> events{
        buttonEvent(RawKind::ButtonDown, MouseButton::Right, 100, 100, 0),
        moveEvent(101, 101, 10),
        moveEvent(102, 102, 20),
        buttonEvent(RawKind::ButtonUp, MouseButton::Right, 102, 102, 50),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, ActionType::MouseClick);
}

TEST(Normalize, DragBeyondThresholdEmitsDownMoveUp) {
    const std::vector<RawEvent> events{
        buttonEvent(RawKind::ButtonDown, MouseButton::Left, 100, 100, 0),
        moveEvent(200, 150, 10),
        moveEvent(210, 160, 20),
        buttonEvent(RawKind::ButtonUp, MouseButton::Left, 210, 160, 30),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 4u);
    EXPECT_EQ(actions[0].type, ActionType::MouseDown);
    EXPECT_EQ(actions[1].type, ActionType::MouseMove);
    EXPECT_EQ(actions[2].type, ActionType::MouseMove);
    EXPECT_EQ(actions[3].type, ActionType::MouseUp);

    EXPECT_EQ(actions[0].x, 100);
    EXPECT_EQ(actions[0].delayMs, 0);
    EXPECT_EQ(actions[1].delayMs, 10);
}

TEST(Normalize, DragEndpointsKeepCoordinates) {
    const std::vector<RawEvent> events{
        buttonEvent(RawKind::ButtonDown, MouseButton::Left, 10, 20, 0),
        moveEvent(100, 100, 5),
        buttonEvent(RawKind::ButtonUp, MouseButton::Left, 300, 400, 9),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 3u);
    EXPECT_EQ(actions[0].type, ActionType::MouseDown);
    EXPECT_EQ(actions[0].x, 10);
    EXPECT_EQ(actions[0].y, 20);
    EXPECT_EQ(actions[2].type, ActionType::MouseUp);
    EXPECT_EQ(actions[2].x, 300);
    EXPECT_EQ(actions[2].y, 400);
}

// ---- 滚轮 ----

TEST(Normalize, WheelAccumulatesToNotches) {
    const std::vector<RawEvent> events{
        wheelEvent(240, 0),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, ActionType::MouseWheel);
    EXPECT_EQ(actions[0].wheelDirection, WheelDirection::Up);
    EXPECT_EQ(actions[0].wheelSteps, 2);
}

TEST(Normalize, WheelRemainderIsKept) {
    const std::vector<RawEvent> events{
        wheelEvent(60, 0),
        wheelEvent(60, 10),
        wheelEvent(-120, 20),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 2u);

    EXPECT_EQ(actions[0].wheelDirection, WheelDirection::Up);
    EXPECT_EQ(actions[0].wheelSteps, 1);
    EXPECT_EQ(actions[0].delayMs, 0);    // 第一条动作，delay 恒为 0

    EXPECT_EQ(actions[1].wheelDirection, WheelDirection::Down);
    EXPECT_EQ(actions[1].wheelSteps, 1);
    EXPECT_EQ(actions[1].delayMs, 10);   // 归到"凑满一格"的那条事件上
}
// ---- 时序与上限 ----

TEST(Normalize, DelayIsRelativeToPreviousAction) {
    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, 65, 0),
        keyEvent(RawKind::KeyUp, 65, 40),
        keyEvent(RawKind::KeyDown, 66, 100),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 3u);
    EXPECT_EQ(actions[0].delayMs, 0);
    EXPECT_EQ(actions[1].delayMs, 40);
    EXPECT_EQ(actions[2].delayMs, 60);
}

TEST(Normalize, MaxActionsStopsEarly) {
    NormalizeOptions opts = plainOpts();
    opts.maxActions = 2;

    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, 65, 0),
        keyEvent(RawKind::KeyDown, 66, 10),
        keyEvent(RawKind::KeyDown, 67, 20),
    };

    const auto actions = recorder::normalize(events, opts);
    EXPECT_EQ(actions.size(), 2u);
}
