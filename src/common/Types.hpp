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

struct TimerModeEntry {
    TimerMode mode;
    std::string_view name;
};

inline constexpr TimerModeEntry kTimerModeEntries[] = {
    {.mode = TimerMode::Sleep, .name = "sleep"},
    {.mode = TimerMode::Spin, .name = "spin"},
};

[[nodiscard]] constexpr TimerMode timerModeFromString(const std::string_view s) noexcept {
    for (const auto &[mode, name]: kTimerModeEntries) {
        if (name == s) return mode;
    }
    return TimerMode::Sleep;
}

[[nodiscard]] constexpr std::string_view timerModeToString(const TimerMode m) noexcept {
    for (const auto &[mode, name]: kTimerModeEntries) {
        if (mode == m) return name;
    }
    return "sleep";
}

// 将来放这里：RunMode（无限/定时/固定次数）、点击类型等
