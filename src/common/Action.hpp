#pragma once

#include <iterator>
#include <string_view>

#include <nlohmann/json.hpp>

#include "common/Types.hpp"

// ============================================================
// 录制动作
// ============================================================

// 动作类型
enum class ActionType {
    MouseMove,
    MouseClick,
    MouseWheel,
    Keyboard,
};

// 鼠标按键
enum class MouseButton {
    Left,
    Right,
    Middle,
    Side1,
    Side2,
};

// 滚轮方向
enum class WheelDirection {
    Up,
    Down,
    Left,
    Right,
};

// 键盘事件类型
enum class KeyEventType {
    Down,
    Up,
};

// ============================================================
// 枚举 ↔ 字符串（表驱动）
// ============================================================

struct ActionTypeEntry {
    ActionType value;
    std::string_view name;
};

inline constexpr ActionTypeEntry kActionTypeEntries[] = {
    {.value = ActionType::MouseMove,  .name = "MouseMove"},
    {.value = ActionType::MouseClick, .name = "MouseClick"},
    {.value = ActionType::MouseWheel, .name = "MouseWheel"},
    {.value = ActionType::Keyboard,   .name = "Keyboard"},
};

struct MouseButtonEntry {
    MouseButton value;
    std::string_view name;
};

inline constexpr MouseButtonEntry kMouseButtonEntries[] = {
    {.value = MouseButton::Left,   .name = "Left"},
    {.value = MouseButton::Right,  .name = "Right"},
    {.value = MouseButton::Middle, .name = "Middle"},
    {.value = MouseButton::Side1,  .name = "Side1"},
    {.value = MouseButton::Side2,  .name = "Side2"},
};

struct WheelDirectionEntry {
    WheelDirection value;
    std::string_view name;
};

inline constexpr WheelDirectionEntry kWheelDirectionEntries[] = {
    {.value = WheelDirection::Up,    .name = "Up"},
    {.value = WheelDirection::Down,  .name = "Down"},
    {.value = WheelDirection::Left,  .name = "Left"},
    {.value = WheelDirection::Right, .name = "Right"},
};

struct KeyEventTypeEntry {
    KeyEventType value;
    std::string_view name;
};

inline constexpr KeyEventTypeEntry kKeyEventTypeEntries[] = {
    {.value = KeyEventType::Down, .name = "Down"},
    {.value = KeyEventType::Up,   .name = "Up"},
};

// ---- 通用查表辅助 ----

template <typename Entry, typename Enum>
[[nodiscard]] constexpr std::string_view enumToString(const Entry *entries,
                                                       const std::size_t count,
                                                       const Enum value) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        if (entries[i].value == value) return entries[i].name;
    }
    return {};
}

template <typename Entry, typename Enum>
[[nodiscard]] constexpr Enum enumFromString(const Entry *entries,
                                            const std::size_t count,
                                            const std::string_view name,
                                            const Enum fallback) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        if (entries[i].name == name) return entries[i].value;
    }
    return fallback;
}

// ---- 具体枚举的便捷封装 ----

[[nodiscard]] constexpr std::string_view actionTypeToString(const ActionType t) noexcept {
    return enumToString(kActionTypeEntries, std::size(kActionTypeEntries), t);
}

[[nodiscard]] constexpr ActionType actionTypeFromString(const std::string_view s) noexcept {
    return enumFromString(kActionTypeEntries, std::size(kActionTypeEntries),
                          s, ActionType::MouseMove);
}

[[nodiscard]] constexpr std::string_view mouseButtonToString(const MouseButton b) noexcept {
    return enumToString(kMouseButtonEntries, std::size(kMouseButtonEntries), b);
}

[[nodiscard]] constexpr MouseButton mouseButtonFromString(const std::string_view s) noexcept {
    return enumFromString(kMouseButtonEntries, std::size(kMouseButtonEntries),
                          s, MouseButton::Left);
}

[[nodiscard]] constexpr std::string_view wheelDirectionToString(const WheelDirection d) noexcept {
    return enumToString(kWheelDirectionEntries, std::size(kWheelDirectionEntries), d);
}

[[nodiscard]] constexpr WheelDirection wheelDirectionFromString(const std::string_view s) noexcept {
    return enumFromString(kWheelDirectionEntries, std::size(kWheelDirectionEntries),
                          s, WheelDirection::Up);
}

[[nodiscard]] constexpr std::string_view keyEventTypeToString(const KeyEventType e) noexcept {
    return enumToString(kKeyEventTypeEntries, std::size(kKeyEventTypeEntries), e);
}

[[nodiscard]] constexpr KeyEventType keyEventTypeFromString(const std::string_view s) noexcept {
    return enumFromString(kKeyEventTypeEntries, std::size(kKeyEventTypeEntries),
                          s, KeyEventType::Down);
}

// ============================================================
// 动作
// ============================================================

// 一个录制动作。
//
// 值类型：可拷贝、可移动。字段固定，由 type 决定哪些字段有意义：
//   MouseMove : x, y, delayMs
//   MouseClick: button, x, y, holdMs, delayMs
//   MouseWheel: wheelDirection, wheelSteps, delayMs
//   Keyboard  : hotkey, keyEvent, delayMs
struct Action {
    ActionType type = ActionType::MouseMove;

    // MouseMove / MouseClick
    int x = 0;
    int y = 0;

    // MouseClick
    MouseButton button = MouseButton::Left;
    int holdMs = 0;

    // MouseWheel
    WheelDirection wheelDirection = WheelDirection::Up;
    int wheelSteps = 1;

    // Keyboard
    Hotkey hotkey;
    KeyEventType keyEvent = KeyEventType::Down;

    // 所有类型都有：距上一个动作的间隔
    int delayMs = 0;
};

// ============================================================
// Action 的 nlohmann ADL 序列化
//
// 形态：按 type 精简，只写有意义的字段
//   MouseMove : {"type": "MouseMove", "x":..., "y":..., "delayMs":...}
//   MouseClick: {"type": "MouseClick", "button":..., "x":..., "y":...,
//                "holdMs":..., "delayMs":...}
//   MouseWheel: {"type": "MouseWheel", "direction":..., "steps":...,
//                "delayMs":...}
//   Keyboard  : {"type": "Keyboard", "key": {...}, "event": "Down",
//                "delayMs":...}
// ============================================================

void to_json(nlohmann::json &j, const Action &a);
void from_json(const nlohmann::json &j, Action &a);
