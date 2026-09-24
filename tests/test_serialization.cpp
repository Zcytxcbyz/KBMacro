// ============================================================
// 序列化 round-trip 测试
//
// 覆盖：Action / Hotkey / RunSpec / ClickerParams /
//       KeyboardParams / ScreenSpec / Config
//
// 每个测试都是 to_json → from_json → 逐字段比较。
// 目的是抓住"改 from_json 漏了一个字段"这类静默 bug。
// ============================================================

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "common/Action.hpp"
#include "common/ClickerParams.hpp"
#include "common/Config.hpp"
#include "common/Hotkey.hpp"
#include "common/KeyboardParams.hpp"
#include "common/Types.hpp"

// ---- Action ----

TEST(ActionRoundTrip, MouseMove) {
    Action a;
    a.type = ActionType::MouseMove;
    a.x = 123;
    a.y = 456;
    a.delayMs = 789;

    const auto j = nlohmann::json(a);
    const Action b = j.get<Action>();

    EXPECT_EQ(b.type, ActionType::MouseMove);
    EXPECT_EQ(b.x, 123);
    EXPECT_EQ(b.y, 456);
    EXPECT_EQ(b.delayMs, 789);
}

TEST(ActionRoundTrip, MouseClick) {
    Action a;
    a.type = ActionType::MouseClick;
    a.button = MouseButton::Right;
    a.x = 10;
    a.y = 20;
    a.holdMs = 30;
    a.delayMs = 40;

    const auto j = nlohmann::json(a);
    const Action b = j.get<Action>();

    EXPECT_EQ(b.type, ActionType::MouseClick);
    EXPECT_EQ(b.button, MouseButton::Right);
    EXPECT_EQ(b.x, 10);
    EXPECT_EQ(b.y, 20);
    EXPECT_EQ(b.holdMs, 30);
    EXPECT_EQ(b.delayMs, 40);
}

TEST(ActionRoundTrip, MouseWheel) {
    Action a;
    a.type = ActionType::MouseWheel;
    a.wheelDirection = WheelDirection::Left;
    a.wheelSteps = 5;
    a.delayMs = 15;

    const auto j = nlohmann::json(a);
    const Action b = j.get<Action>();

    EXPECT_EQ(b.type, ActionType::MouseWheel);
    EXPECT_EQ(b.wheelDirection, WheelDirection::Left);
    EXPECT_EQ(b.wheelSteps, 5);
    EXPECT_EQ(b.delayMs, 15);
}

TEST(ActionRoundTrip, Keyboard) {
    Action a;
    a.type = ActionType::Keyboard;
    a.hotkey.modifiers = hk::kModCtrl | hk::kModShift;
    a.hotkey.vk = 0x70;  // F1
    a.keyEvent = KeyEventType::Up;
    a.delayMs = 99;

    const auto j = nlohmann::json(a);
    const Action b = j.get<Action>();

    EXPECT_EQ(b.type, ActionType::Keyboard);
    EXPECT_EQ(b.hotkey.modifiers, hk::kModCtrl | hk::kModShift);
    EXPECT_EQ(b.hotkey.vk, 0x70u);
    EXPECT_EQ(b.keyEvent, KeyEventType::Up);
    EXPECT_EQ(b.delayMs, 99);
}

// ---- Hotkey ----

TEST(HotkeyRoundTrip, WithModifiersAndKey) {
    Hotkey hk;
    hk.modifiers = hk::kModCtrl | hk::kModAlt;
    hk.vk = 0x41;  // A

    const auto j = nlohmann::json(hk);
    const Hotkey b = j.get<Hotkey>();

    EXPECT_EQ(b.modifiers, hk::kModCtrl | hk::kModAlt);
    EXPECT_EQ(b.vk, 0x41u);
}

TEST(HotkeyRoundTrip, Empty) {
    const Hotkey hk;
    const auto j = nlohmann::json(hk);
    const Hotkey b = j.get<Hotkey>();

    EXPECT_EQ(b.modifiers, 0u);
    EXPECT_EQ(b.vk, 0u);
}

TEST(HotkeyRoundTrip, OnlyModifiers) {
    Hotkey hk;
    hk.modifiers = hk::kModWin;

    const auto j = nlohmann::json(hk);
    const Hotkey b = j.get<Hotkey>();

    EXPECT_EQ(b.modifiers, hk::kModWin);
    EXPECT_EQ(b.vk, 0u);
}

// ---- RunSpec ----

TEST(RunSpecRoundTrip, Infinite) {
    RunSpec r;
    r.mode = RunMode::Infinite;

    const auto j = nlohmann::json(r);
    const RunSpec b = j.get<RunSpec>();

    EXPECT_EQ(b.mode, RunMode::Infinite);
}

TEST(RunSpecRoundTrip, Timed) {
    RunSpec r;
    r.mode = RunMode::Timed;
    r.durationSec = 42;

    const auto j = nlohmann::json(r);
    const RunSpec b = j.get<RunSpec>();

    EXPECT_EQ(b.mode, RunMode::Timed);
    EXPECT_EQ(b.durationSec, 42);
}

TEST(RunSpecRoundTrip, Counted) {
    RunSpec r;
    r.mode = RunMode::Counted;
    r.count = 777;

    const auto j = nlohmann::json(r);
    const RunSpec b = j.get<RunSpec>();

    EXPECT_EQ(b.mode, RunMode::Counted);
    EXPECT_EQ(b.count, 777);
}

// ---- ScreenSpec ----

TEST(ScreenSpecRoundTrip, AllFields) {
    ScreenSpec s;
    s.width = 1920;
    s.height = 1080;
    s.dpi = 96;

    const auto j = nlohmann::json(s);
    const ScreenSpec b = j.get<ScreenSpec>();

    EXPECT_EQ(b.width, 1920);
    EXPECT_EQ(b.height, 1080);
    EXPECT_EQ(b.dpi, 96);
}

// ---- ClickerParams ----

TEST(ClickerParamsRoundTrip, AllFields) {
    ClickerParams p;
    p.ignorePosition = true;
    p.x = -10;
    p.y = 20;
    p.clickType = ClickType::Double;
    p.wheelSteps = 3;
    p.intervalMs = 500;
    p.holdMs = 80;
    p.run.mode = RunMode::Timed;
    p.run.durationSec = 15;

    const auto j = nlohmann::json(p);
    const ClickerParams b = j.get<ClickerParams>();

    EXPECT_EQ(b.ignorePosition, true);
    EXPECT_EQ(b.x, -10);
    EXPECT_EQ(b.y, 20);
    EXPECT_EQ(b.clickType, ClickType::Double);
    EXPECT_EQ(b.wheelSteps, 3);
    EXPECT_EQ(b.intervalMs, 500);
    EXPECT_EQ(b.holdMs, 80);
    EXPECT_EQ(b.run.mode, RunMode::Timed);
    EXPECT_EQ(b.run.durationSec, 15);
}

// ---- KeyboardParams ----

TEST(KeyboardParamsRoundTrip, AllFields) {
    KeyboardParams p;
    p.hotkey.modifiers = hk::kModCtrl;
    p.hotkey.vk = 0x0D;  // Enter
    p.intervalMs = 250;
    p.holdMs = 60;
    p.run.mode = RunMode::Counted;
    p.run.count = 999;

    const auto j = nlohmann::json(p);
    const KeyboardParams b = j.get<KeyboardParams>();

    EXPECT_EQ(b.hotkey.modifiers, hk::kModCtrl);
    EXPECT_EQ(b.hotkey.vk, 0x0Du);
    EXPECT_EQ(b.intervalMs, 250);
    EXPECT_EQ(b.holdMs, 60);
    EXPECT_EQ(b.run.mode, RunMode::Counted);
    EXPECT_EQ(b.run.count, 999);
}

// ---- Config ----

TEST(ConfigRoundTrip, TopLevelFields) {
    Config c;
    c.language = "en-US";
    c.timerMode = TimerMode::Spin;
    c.alwaysOnTop = true;
    c.tabIndex = 3;

    const auto j = nlohmann::json(c);
    const Config b = j.get<Config>();

    EXPECT_EQ(b.language, "en-US");
    EXPECT_EQ(b.timerMode, TimerMode::Spin);
    EXPECT_EQ(b.alwaysOnTop, true);
    EXPECT_EQ(b.tabIndex, 3);
}

TEST(ConfigRoundTrip, HotkeysAndEnabled) {
    Config c;
    c.hotkeys[0].modifiers = hk::kModCtrl;
    c.hotkeys[0].vk = 0x70;  // F1
    c.hotkeys[1].modifiers = hk::kModAlt;
    c.hotkeys[1].vk = 0x71;  // F2
    c.hotkeyEnabled[0] = true;
    c.hotkeyEnabled[1] = false;

    const auto j = nlohmann::json(c);
    const Config b = j.get<Config>();

    EXPECT_EQ(b.hotkeys[0].modifiers, hk::kModCtrl);
    EXPECT_EQ(b.hotkeys[0].vk, 0x70u);
    EXPECT_EQ(b.hotkeys[1].modifiers, hk::kModAlt);
    EXPECT_EQ(b.hotkeys[1].vk, 0x71u);
    EXPECT_EQ(b.hotkeyEnabled[0], true);
    EXPECT_EQ(b.hotkeyEnabled[1], false);
}

TEST(ConfigRoundTrip, WindowAndFlags) {
    Config c;
    c.window.x = 100;
    c.window.y = 200;
    c.window.width = 800;
    c.window.height = 600;
    c.window.maximized = true;
    c.flags.spinWarningConfirmed = true;
    c.flags.adaptationPreference = AdaptationPreference::AlwaysAdapt;

    const auto j = nlohmann::json(c);
    const Config b = j.get<Config>();

    EXPECT_EQ(b.window.x, 100);
    EXPECT_EQ(b.window.y, 200);
    EXPECT_EQ(b.window.width, 800);
    EXPECT_EQ(b.window.height, 600);
    EXPECT_EQ(b.window.maximized, true);
    EXPECT_EQ(b.flags.spinWarningConfirmed, true);
    EXPECT_EQ(b.flags.adaptationPreference, AdaptationPreference::AlwaysAdapt);
}

TEST(ConfigRoundTrip, MissingFieldsUseDefaults) {
    // 模拟一个"老配置"：只有 language，没有 window / flags / hotkeys
    const nlohmann::json j = {
        {"version", 1},
        {"language", "zh-CN"},
    };

    const Config b = j.get<Config>();

    EXPECT_EQ(b.language, "zh-CN");
    EXPECT_EQ(b.timerMode, TimerMode::Sleep);
    EXPECT_EQ(b.alwaysOnTop, false);
    EXPECT_EQ(b.window.x, -1);
    EXPECT_EQ(b.window.width, 650);   // 与 Config.hpp 一致
    EXPECT_EQ(b.window.height, 720);
    EXPECT_EQ(b.flags.spinWarningConfirmed, false);
    EXPECT_EQ(b.flags.adaptationPreference, AdaptationPreference::Ask);
    // hotkeyEnabled 默认全 true
    for (const bool e: b.hotkeyEnabled) {
        EXPECT_TRUE(e);
    }
}
