#pragma once

#include <string_view>

// ============================================================
// 跨模块共享的枚举与常量
// ============================================================

// 计时器模式：由 ConfigManager 持久化，ITimer 消费
enum class TimerMode {
    Sleep,
    Spin
};

[[nodiscard]] constexpr TimerMode timerModeFromString(const std::string_view s) noexcept {
    return s == "spin" ? TimerMode::Spin : TimerMode::Sleep;
}

[[nodiscard]] constexpr std::string_view timerModeToString(const TimerMode m) noexcept {
    return m == TimerMode::Spin ? "spin" : "sleep";
}

// 将来放这里：RunMode（无限/定时/固定次数）、点击类型等
