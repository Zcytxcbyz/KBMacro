#pragma once

#include <iterator>
#include <string_view>

#include <nlohmann/json.hpp>

#include "common/Types.hpp"

// ============================================================
// recorded actions
// ============================================================

// action type
enum class ActionType {
    MouseMove,
    MouseClick,
    MouseWheel,
    Keyboard,
    // the two atomic actions used for dragging: any number of MouseMove fits between Down and Up
    // a click in place still yields MouseClick (one action); only a real drag splits into Down/Move.../Up
    MouseDown,
    MouseUp,
    // pure wait: emits no input event, just pauses for delayMs (delayMs is a duration for this type)
    Delay,
    // one press-and-release: down, hold for holdMs, up. Recording collapses a key down/up pair into
    // this when nothing else happened in between; Keyboard/Down + Keyboard/Up stay for chords and holds.
    // Always appended last: the type combo box indexes by enum value.
    KeyboardPress,
};

// mouse buttons
enum class MouseButton {
    Left,
    Right,
    Middle,
    Side1,
    Side2,
};

// wheel direction
enum class WheelDirection {
    Up,
    Down,
    Left,
    Right,
};

// keyboard event type
enum class KeyEventType {
    Down,
    Up,
};

// ============================================================
// enum <-> string (table-driven)
// ============================================================

struct ActionTypeEntry {
    ActionType value;
    std::string_view name;
};

inline constexpr ActionTypeEntry kActionTypeEntries[] = {
        {.value = ActionType::MouseMove, .name = "MouseMove"},
        {.value = ActionType::MouseClick, .name = "MouseClick"},
        {.value = ActionType::MouseWheel, .name = "MouseWheel"},
        {.value = ActionType::Keyboard, .name = "Keyboard"},
        {.value = ActionType::MouseDown, .name = "MouseDown"},
        {.value = ActionType::MouseUp, .name = "MouseUp"},
        {.value = ActionType::Delay, .name = "Delay"},
        {.value = ActionType::KeyboardPress, .name = "KeyboardPress"},
};

struct MouseButtonEntry {
    MouseButton value;
    std::string_view name;
};

inline constexpr MouseButtonEntry kMouseButtonEntries[] = {
        {.value = MouseButton::Left, .name = "Left"},
        {.value = MouseButton::Right, .name = "Right"},
        {.value = MouseButton::Middle, .name = "Middle"},
        {.value = MouseButton::Side1, .name = "Side1"},
        {.value = MouseButton::Side2, .name = "Side2"},
};

struct WheelDirectionEntry {
    WheelDirection value;
    std::string_view name;
};

inline constexpr WheelDirectionEntry kWheelDirectionEntries[] = {
        {.value = WheelDirection::Up, .name = "Up"},
        {.value = WheelDirection::Down, .name = "Down"},
        {.value = WheelDirection::Left, .name = "Left"},
        {.value = WheelDirection::Right, .name = "Right"},
};

struct KeyEventTypeEntry {
    KeyEventType value;
    std::string_view name;
};

inline constexpr KeyEventTypeEntry kKeyEventTypeEntries[] = {
        {.value = KeyEventType::Down, .name = "Down"},
        {.value = KeyEventType::Up, .name = "Up"},
};

// ---- generic table lookup helpers ----

template<typename Entry, typename Enum>
[[nodiscard]] constexpr std::string_view
enumToString(const Entry *entries, const std::size_t count, const Enum value) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        if (entries[i].value == value) return entries[i].name;
    }
    return {};
}

template<typename Entry, typename Enum>
[[nodiscard]] constexpr Enum enumFromString(const Entry *entries,
                                            const std::size_t count,
                                            const std::string_view name,
                                            const Enum fallback) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        if (entries[i].name == name) return entries[i].value;
    }
    return fallback;
}

// ---- convenience wrappers for concrete enums ----

[[nodiscard]] constexpr std::string_view actionTypeToString(const ActionType t) noexcept {
    return enumToString(kActionTypeEntries, std::size(kActionTypeEntries), t);
}

[[nodiscard]] constexpr ActionType actionTypeFromString(const std::string_view s) noexcept {
    return enumFromString(kActionTypeEntries, std::size(kActionTypeEntries), s, ActionType::MouseMove);
}

[[nodiscard]] constexpr std::string_view mouseButtonToString(const MouseButton b) noexcept {
    return enumToString(kMouseButtonEntries, std::size(kMouseButtonEntries), b);
}

[[nodiscard]] constexpr MouseButton mouseButtonFromString(const std::string_view s) noexcept {
    return enumFromString(kMouseButtonEntries, std::size(kMouseButtonEntries), s, MouseButton::Left);
}

[[nodiscard]] constexpr std::string_view wheelDirectionToString(const WheelDirection d) noexcept {
    return enumToString(kWheelDirectionEntries, std::size(kWheelDirectionEntries), d);
}

[[nodiscard]] constexpr WheelDirection wheelDirectionFromString(const std::string_view s) noexcept {
    return enumFromString(kWheelDirectionEntries, std::size(kWheelDirectionEntries), s, WheelDirection::Up);
}

[[nodiscard]] constexpr std::string_view keyEventTypeToString(const KeyEventType e) noexcept {
    return enumToString(kKeyEventTypeEntries, std::size(kKeyEventTypeEntries), e);
}

[[nodiscard]] constexpr KeyEventType keyEventTypeFromString(const std::string_view s) noexcept {
    return enumFromString(kKeyEventTypeEntries, std::size(kKeyEventTypeEntries), s, KeyEventType::Down);
}

// value type: copyable and movable. Fields are fixed; type decides which ones matter:
//   MouseMove  : x, y, delayMs
//   MouseClick : button, x, y, holdMs, delayMs
//   MouseWheel : wheelDirection, wheelSteps, delayMs
//   Keyboard   : hotkey, keyEvent, delayMs
//   MouseDown  : button, x, y, delayMs
//   MouseUp    : button, x, y, delayMs
//   Delay      : delayMs (here delayMs is the wait duration, not the "pre-action interval")
//   KeyboardPress: hotkey, holdMs, delayMs
struct Action {
    ActionType type = ActionType::MouseMove;

    // coordinates of mouse actions.
    //   relative == false: absolute screen coordinates (virtual desktop, physical pixels)
    //   relative == true : offset from the current cursor position (may be negative), x/y are dx/dy
    // recording always yields absolute coordinates; relative is only for hand-written scripts.
    int x = 0;
    int y = 0;
    bool relative = false;

    // MouseClick / KeyboardPress
    MouseButton button = MouseButton::Left;
    int holdMs = 0;

    // MouseWheel
    WheelDirection wheelDirection = WheelDirection::Up;
    int wheelSteps = 1;

    // Keyboard
    Hotkey hotkey;
    KeyEventType keyEvent = KeyEventType::Down;

    // present for every type: interval since the previous action
    int delayMs = 0;
};

// ============================================================
// nlohmann ADL serialization for Action
//
// shape: trimmed per type, only meaningful fields are written; optional fields at default are omitted
//   MouseMove  : {"type": "MouseMove", "x":..., "y":..., "delayMs":...}
//   MouseClick : {"type": "MouseClick", "button":..., "x":..., "y":...,
//                 "holdMs":..., "delayMs":..., "relative": true(optional)}
//   MouseWheel : {"type": "MouseWheel", "direction":..., "steps":...,
//                 "delayMs":...}
//   Keyboard   : {"type": "Keyboard", "key": {...}, "event": "Down",
//                 "delayMs":...}
//   KeyboardPress : {"type": "KeyboardPress", "key": {...},
//                    "holdMs":..., "delayMs":...},
//   MouseDown / MouseUp : {"type": ..., "button":..., "x":..., "y":...,
//                          "delayMs":..., "relative": true(optional)}
//   Delay      : {"type": "Delay", "delayMs":...}
// ============================================================

void to_json(nlohmann::json &j, const Action &a);
void from_json(const nlohmann::json &j, Action &a);
