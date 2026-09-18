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

// ============================================================
// 热键
// ============================================================

// 修饰键位掩码
namespace hk {
    constexpr unsigned kModCtrl  = 1u << 0;
    constexpr unsigned kModShift = 1u << 1;
    constexpr unsigned kModAlt   = 1u << 2;
    constexpr unsigned kModWin   = 1u << 3;
}

// 一个按键组合：修饰键位掩码 + 主键虚拟键码
// vk == 0 表示"未设置主键"（可能只在等待用户继续按主键）
struct Hotkey {
    unsigned modifiers = 0;
    unsigned vk = 0;
};

// 将来放这里：RunMode（无限/定时/固定次数）、点击类型等
