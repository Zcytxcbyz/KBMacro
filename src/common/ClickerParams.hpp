#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "common/Types.hpp"

// ============================================================
// 连点器参数
//
// 点击类型 10 项，与 ClickerPanel 的下拉框 1:1 对应。
// 不复用 Action 的枚举——连点器和录制是两套语义，不互转。
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
    {.value = ClickType::Left,       .name = "Left"},
    {.value = ClickType::Right,      .name = "Right"},
    {.value = ClickType::Middle,     .name = "Middle"},
    {.value = ClickType::Double,     .name = "Double"},
    {.value = ClickType::WheelUp,    .name = "WheelUp"},
    {.value = ClickType::WheelDown,  .name = "WheelDown"},
    {.value = ClickType::WheelLeft,  .name = "WheelLeft"},
    {.value = ClickType::WheelRight, .name = "WheelRight"},
    {.value = ClickType::Side1,      .name = "Side1"},
    {.value = ClickType::Side2,      .name = "Side2"},
};

[[nodiscard]] constexpr std::string_view clickTypeToString(const ClickType t) noexcept {
    for (const auto &[v, n]: kClickTypeEntries) {
        if (v == t) return n;
    }
    return "Left";
}

[[nodiscard]] constexpr ClickType clickTypeFromString(const std::string_view s) noexcept {
    for (const auto &[v, n]: kClickTypeEntries) {
        if (n == s) return v;
    }
    return ClickType::Left;
}

// 是否是滚轮类点击
[[nodiscard]] constexpr bool isWheelClickType(const ClickType t) noexcept {
    return t == ClickType::WheelUp || t == ClickType::WheelDown
           || t == ClickType::WheelLeft || t == ClickType::WheelRight;
}

// ============================================================
// 参数
// ============================================================

struct ClickerParams {
    // 位置
    bool ignorePosition = false;   // 勾选后忽略 x/y，用当前光标位置
    int x = 0;
    int y = 0;

    // 点击
    ClickType clickType = ClickType::Left;
    int wheelSteps = 1;            // 仅滚轮类有效

    // 时间
    int intervalMs = 100;
    int holdMs = 50;               // 仅非滚轮类有效

    // 运行模式
    RunSpec run;
};

void to_json(nlohmann::json &j, const ClickerParams &p);
void from_json(const nlohmann::json &j, ClickerParams &p);
