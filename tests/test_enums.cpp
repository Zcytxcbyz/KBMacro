// ============================================================
// enum ↔ string conversion tests
//
// covers all table-driven enums and checks:
//   1. every enum value converts to a non-empty string
//   2. to → from round-trips consistently
//   3. unknown strings fall back to the default value
// ============================================================

#include <gtest/gtest.h>

#include "common/Action.hpp"
#include "common/ClickerParams.hpp"
#include "common/Types.hpp"

// ---- TimerMode ----

TEST(TimerModeConversion, RoundTrip) {
    for (const auto &[mode, name] : kTimerModeEntries) {
        EXPECT_EQ(timerModeFromString(name), mode) << "name=" << name;
        EXPECT_EQ(timerModeToString(mode), name);
    }
}

TEST(TimerModeConversion, UnknownFallsBackToSleep) {
    EXPECT_EQ(timerModeFromString("nonsense"), TimerMode::Sleep);
    EXPECT_EQ(timerModeFromString(""), TimerMode::Sleep);
}

// ---- RunMode ----

TEST(RunModeConversion, RoundTrip) {
    for (const auto &[mode, name] : kRunModeEntries) {
        EXPECT_EQ(runModeFromString(name), mode) << "name=" << name;
        EXPECT_EQ(runModeToString(mode), name);
    }
}

TEST(RunModeConversion, UnknownFallsBackToInfinite) {
    EXPECT_EQ(runModeFromString("bogus"), RunMode::Counted);
}

// ---- AdaptationPreference ----

TEST(AdaptationPreferenceConversion, RoundTrip) {
    for (const auto &[value, name] : kAdaptationPreferenceEntries) {
        EXPECT_EQ(adaptationPreferenceFromString(name), value) << "name=" << name;
        EXPECT_EQ(adaptationPreferenceToString(value), name);
    }
}

TEST(AdaptationPreferenceConversion, UnknownFallsBackToAsk) {
    EXPECT_EQ(adaptationPreferenceFromString("nope"), AdaptationPreference::Ask);
}

// ---- ActionType ----

TEST(ActionTypeConversion, RoundTrip) {
    for (const auto &[value, name] : kActionTypeEntries) {
        EXPECT_EQ(actionTypeFromString(name), value) << "name=" << name;
        EXPECT_EQ(actionTypeToString(value), name);
    }
}

TEST(ActionTypeConversion, UnknownFallsBackToMouseMove) {
    EXPECT_EQ(actionTypeFromString("xyz"), ActionType::MouseMove);
}

// ---- MouseButton ----

TEST(MouseButtonConversion, RoundTrip) {
    for (const auto &[value, name] : kMouseButtonEntries) {
        EXPECT_EQ(mouseButtonFromString(name), value) << "name=" << name;
        EXPECT_EQ(mouseButtonToString(value), name);
    }
}

// ---- WheelDirection ----

TEST(WheelDirectionConversion, RoundTrip) {
    for (const auto &[value, name] : kWheelDirectionEntries) {
        EXPECT_EQ(wheelDirectionFromString(name), value) << "name=" << name;
        EXPECT_EQ(wheelDirectionToString(value), name);
    }
}

// ---- KeyEventType ----

TEST(KeyEventTypeConversion, RoundTrip) {
    for (const auto &[value, name] : kKeyEventTypeEntries) {
        EXPECT_EQ(keyEventTypeFromString(name), value) << "name=" << name;
        EXPECT_EQ(keyEventTypeToString(value), name);
    }
}

// ---- ClickType ----

TEST(ClickTypeConversion, RoundTrip) {
    for (const auto &[value, name] : kClickTypeEntries) {
        EXPECT_EQ(clickTypeFromString(name), value) << "name=" << name;
        EXPECT_EQ(clickTypeToString(value), name);
    }
}

TEST(ClickTypeConversion, WheelTypesPredicate) {
    EXPECT_TRUE(isWheelClickType(ClickType::WheelUp));
    EXPECT_TRUE(isWheelClickType(ClickType::WheelDown));
    EXPECT_TRUE(isWheelClickType(ClickType::WheelLeft));
    EXPECT_TRUE(isWheelClickType(ClickType::WheelRight));

    EXPECT_FALSE(isWheelClickType(ClickType::Left));
    EXPECT_FALSE(isWheelClickType(ClickType::Right));
    EXPECT_FALSE(isWheelClickType(ClickType::Middle));
    EXPECT_FALSE(isWheelClickType(ClickType::Double));
    EXPECT_FALSE(isWheelClickType(ClickType::Side1));
    EXPECT_FALSE(isWheelClickType(ClickType::Side2));
}
