// ============================================================
// Normalization pure-function tests:
//   - keyboard: no modifiers, auto-repeat merged, Up without Down dropped
//   - mouse: click vs drag (2px threshold), drag endpoints, multiple buttons at once
//   - wheel: accumulate into whole notches, keep the remainder
//   - general: trim head/tail modifiers, filter main keys, delayMs, action cap
// Hooks and the message pump are not tested.
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

// this test file does not include <windows.h>, so the VK values are written directly
constexpr unsigned kVkShift = 0x10; // VK_SHIFT
constexpr unsigned kVkControl = 0x11; // VK_CONTROL

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

RawEvent buttonEvent(const RawKind kind, const MouseButton button, const int x, const int y, const std::uint32_t t) {
    return RawEvent{.kind = kind, .x = x, .y = y, .button = button, .time = t};
}

RawEvent wheelEvent(const int delta, const std::uint32_t t, const bool horizontal = false) {
    return RawEvent{.kind = RawKind::Wheel, .wheelDelta = delta, .time = t, .horizontal = horizontal};
}

} // namespace

// ---- empty input ----

TEST(Normalize, EmptyInputYieldsNothing) {
    EXPECT_TRUE(recorder::normalize({}, plainOpts()).empty());
}

// ---- keyboard ----

TEST(Normalize, KeyPairBecomesPress) {
    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, 65, 0), // A
        keyEvent(RawKind::KeyUp, 65, 30),
};

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 1u);

    EXPECT_EQ(actions[0].type, ActionType::KeyboardPress);
    EXPECT_EQ(actions[0].hotkey.vk, 65u);
    EXPECT_EQ(actions[0].holdMs, 30);
    EXPECT_EQ(actions[0].delayMs, 0);
}

TEST(Normalize, KeyboardActionsCarryNoModifiers) {
    const std::vector<RawEvent> events{
            keyEvent(RawKind::KeyDown, 65, 0),
            keyEvent(RawKind::KeyUp, 65, 10),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 1u);

    // chords are expressed by the event sequence, a single action's modifiers must always be 0
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
    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, ActionType::KeyboardPress);
    EXPECT_EQ(actions[0].holdMs, 40); // measured from the first Down, repeats are folded in
}

TEST(Normalize, StartAndStopMainKeysAreDropped) {
    NormalizeOptions opts = plainOpts();
    opts.startKeyVk = 0x70; // F1
    opts.stopKeyVk = 0x71; // F2

    const std::vector<RawEvent> events{
            keyEvent(RawKind::KeyDown, 0x70, 0),
            keyEvent(RawKind::KeyDown, 65, 5),
            keyEvent(RawKind::KeyUp, 65, 10),
            keyEvent(RawKind::KeyUp, 0x70, 15),
            keyEvent(RawKind::KeyDown, 0x71, 20),
    };

    const auto actions = recorder::normalize(events, opts);
    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].type, ActionType::KeyboardPress);
    EXPECT_EQ(actions[0].hotkey.vk, 65u);
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
    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].delayMs, 0); // reset to zero after trimming
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
    ASSERT_EQ(actions.size(), 1u);
}

TEST(Normalize, TrimStopsAtFirstRealAction) {
    NormalizeOptions opts = plainOpts();
    opts.startMods = hk::kModCtrl;

    const std::vector<RawEvent> events{
            keyEvent(RawKind::KeyUp, kVkControl, 0), // head modifier, trimmed
            keyEvent(RawKind::KeyDown, 65, 5),
            keyEvent(RawKind::KeyUp, 65, 10),
            keyEvent(RawKind::KeyDown, kVkControl, 15), // a real Ctrl press by the user, kept
            keyEvent(RawKind::KeyUp, kVkControl, 20),
    };

    const auto actions = recorder::normalize(events, opts);
    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0].type, ActionType::KeyboardPress); // A
    EXPECT_EQ(actions[1].hotkey.vk, kVkControl);           // a real Ctrl press by the user, kept
}

TEST(Normalize, InterruptedPairIsNotFolded) {
    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, 65, 0),
        moveEvent(500, 500, 10), // anything emitted between Down and Up closes the fold window
        keyEvent(RawKind::KeyUp, 65, 20),
};

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 3u);
    EXPECT_EQ(actions[0].type, ActionType::Keyboard);
    EXPECT_EQ(actions[0].keyEvent, KeyEventType::Down);
    EXPECT_EQ(actions[2].keyEvent, KeyEventType::Up);
    EXPECT_EQ(actions[2].delayMs, 10);
}

TEST(Normalize, ChordIsNotFolded) {
    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, kVkControl, 0),
        keyEvent(RawKind::KeyDown, 67, 10), // C
        keyEvent(RawKind::KeyUp, 67, 20),
        keyEvent(RawKind::KeyUp, kVkControl, 30),
};

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 3u);

    EXPECT_EQ(actions[0].type, ActionType::Keyboard); // Ctrl is held across other events
    EXPECT_EQ(actions[0].keyEvent, KeyEventType::Down);
    EXPECT_EQ(actions[1].type, ActionType::KeyboardPress); // only the inner pair folds
    EXPECT_EQ(actions[1].hotkey.vk, 67u);
    EXPECT_EQ(actions[1].holdMs, 10);
    EXPECT_EQ(actions[2].keyEvent, KeyEventType::Up);
}

TEST(Normalize, VeryLongHoldIsNotFolded) {
    const std::vector<RawEvent> events{
        keyEvent(RawKind::KeyDown, 65, 0),
        keyEvent(RawKind::KeyUp, 65, 3600001), // beyond kMaxFoldHoldMs
};

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0].type, ActionType::Keyboard);
    EXPECT_EQ(actions[1].keyEvent, KeyEventType::Up);
}

// ---- mouse: click vs drag ----

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

// ---- wheel ----

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
    EXPECT_EQ(actions[0].delayMs, 0); // the first action, delay is always 0

    EXPECT_EQ(actions[1].wheelDirection, WheelDirection::Down);
    EXPECT_EQ(actions[1].wheelSteps, 1);
    EXPECT_EQ(actions[1].delayMs, 10); // attributed to the event that completed the notch
}

// ---- timing and cap ----

TEST(Normalize, DelayIsRelativeToPreviousAction) {
    const std::vector<RawEvent> events{
            keyEvent(RawKind::KeyDown, 65, 0),
            keyEvent(RawKind::KeyUp, 65, 40),
            keyEvent(RawKind::KeyDown, 66, 100),
    };

    const auto actions = recorder::normalize(events, plainOpts());
    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0].type, ActionType::KeyboardPress);
    EXPECT_EQ(actions[0].delayMs, 0);
    EXPECT_EQ(actions[1].type, ActionType::Keyboard); // B has no Up to fold with
    EXPECT_EQ(actions[1].delayMs, 60);
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
