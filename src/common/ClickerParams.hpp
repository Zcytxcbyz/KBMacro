#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "common/Types.hpp"

// ============================================================
// clicker parameters
//
// 10 click types, mapped 1:1 to the ClickerPanel drop-down.
// Action's enums are not reused: the clicker and recording are two separate domains, no conversion.
// ============================================================

enum class ClickType {
    Left,
    Right,
    Middle,
    Double,
    WheelUp,
    WheelDown,
    WheelLeft,
    WheelRight,
    Side1,
    Side2,
};

struct ClickTypeEntry {
    ClickType value;
    std::string_view name;
};

inline constexpr ClickTypeEntry kClickTypeEntries[] = {
        {.value = ClickType::Left, .name = "Left"},
        {.value = ClickType::Right, .name = "Right"},
        {.value = ClickType::Middle, .name = "Middle"},
        {.value = ClickType::Double, .name = "Double"},
        {.value = ClickType::WheelUp, .name = "WheelUp"},
        {.value = ClickType::WheelDown, .name = "WheelDown"},
        {.value = ClickType::WheelLeft, .name = "WheelLeft"},
        {.value = ClickType::WheelRight, .name = "WheelRight"},
        {.value = ClickType::Side1, .name = "Side1"},
        {.value = ClickType::Side2, .name = "Side2"},
};

[[nodiscard]] constexpr std::string_view clickTypeToString(const ClickType t) noexcept {
    for (const auto &[v, n] : kClickTypeEntries) {
        if (v == t) return n;
    }
    return "Left";
}

[[nodiscard]] constexpr ClickType clickTypeFromString(const std::string_view s) noexcept {
    for (const auto &[v, n] : kClickTypeEntries) {
        if (n == s) return v;
    }
    return ClickType::Left;
}

// whether this is a wheel click type
[[nodiscard]] constexpr bool isWheelClickType(const ClickType t) noexcept {
    return t == ClickType::WheelUp || t == ClickType::WheelDown || t == ClickType::WheelLeft ||
           t == ClickType::WheelRight;
}

// ============================================================
// parameters
// ============================================================

struct ClickerParams {
    // position
    bool ignorePosition = false; // when checked x/y are ignored and the current cursor position is used
    int x = 0;
    int y = 0;

    // click
    ClickType clickType = ClickType::Left;
    int wheelSteps = 1; // only valid for wheel click types

    // timing
    int intervalMs = 100;
    int holdMs = 50; // only valid for non-wheel click types

    // run mode
    RunSpec run;
};

void to_json(nlohmann::json &j, const ClickerParams &p);
void from_json(const nlohmann::json &j, ClickerParams &p);
