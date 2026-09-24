#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

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

// ============================================================
// 运行模式
// ============================================================

enum class RunMode {
    Infinite,
    Timed,
    Counted,
};

struct RunModeEntry {
    RunMode mode;
    std::string_view name;
};

inline constexpr RunModeEntry kRunModeEntries[] = {
    {.mode = RunMode::Infinite, .name = "Infinite"},
    {.mode = RunMode::Timed,    .name = "Timed"},
    {.mode = RunMode::Counted,  .name = "Counted"},
};

[[nodiscard]] constexpr RunMode runModeFromString(const std::string_view s) noexcept {
    for (const auto &[mode, name]: kRunModeEntries) {
        if (name == s) return mode;
    }
    return RunMode::Infinite;
}

[[nodiscard]] constexpr std::string_view runModeToString(const RunMode m) noexcept {
    for (const auto &[mode, name]: kRunModeEntries) {
        if (mode == m) return name;
    }
    return "Infinite";
}

// 运行模式参数：无限 / 定时 / 固定次数
struct RunSpec {
    RunMode mode = RunMode::Infinite;
    int durationSec = 10;   // mode == Timed 时有效
    int count = 100;        // mode == Counted 时有效
};

// 序列化：按 mode 精简
inline void to_json(nlohmann::json &j, const RunSpec &r) {
    j = nlohmann::json::object();
    j["mode"] = runModeToString(r.mode);

    switch (r.mode) {
        case RunMode::Infinite:
            break;
        case RunMode::Timed:
            j["durationSec"] = r.durationSec;
            break;
        case RunMode::Counted:
            j["count"] = r.count;
            break;
    }
}

inline void from_json(const nlohmann::json &j, RunSpec &r) {
    r = RunSpec{};
    r.mode = runModeFromString(j.value("mode", std::string{"Infinite"}));
    r.durationSec = j.value("durationSec", 10);
    r.count = j.value("count", 100);
}

// ============================================================
// 屏幕规格
// ============================================================

// 记录录制/导出时的屏幕环境，用于导入脚本时判断是否需要坐标适配。
//   - width / height: 主屏像素分辨率
//   - dpi: 主屏 DPI（96 = 100%，120 = 125%，144 = 150%）
// 全 0 表示"未记录"（老脚本或非 KBMacro 生成的文件）。
struct ScreenSpec {
    int width = 0;
    int height = 0;
    int dpi = 0;
};

inline void to_json(nlohmann::json &j, const ScreenSpec &s) {
    j = nlohmann::json::object();
    j["width"] = s.width;
    j["height"] = s.height;
    j["dpi"] = s.dpi;
}

inline void from_json(const nlohmann::json &j, ScreenSpec &s) {
    s = ScreenSpec{};
    s.width = j.value("width", 0);
    s.height = j.value("height", 0);
    s.dpi = j.value("dpi", 0);
}

// ============================================================
// 坐标适配偏好
// ============================================================

// 导入脚本遇到屏幕规格不一致时的处理偏好。
// Ask 每次弹窗；其余两项直接执行，不弹窗。
enum class AdaptationPreference {
    Ask,
    AlwaysAdapt,
    KeepOriginal,
};

struct AdaptationPreferenceEntry {
    AdaptationPreference value;
    std::string_view name;
};

inline constexpr AdaptationPreferenceEntry kAdaptationPreferenceEntries[] = {
    {.value = AdaptationPreference::Ask,          .name = "Ask"},
    {.value = AdaptationPreference::AlwaysAdapt,  .name = "AlwaysAdapt"},
    {.value = AdaptationPreference::KeepOriginal, .name = "KeepOriginal"},
};

[[nodiscard]] constexpr AdaptationPreference
adaptationPreferenceFromString(const std::string_view s) noexcept {
    for (const auto &[v, n]: kAdaptationPreferenceEntries) {
        if (n == s) return v;
    }
    return AdaptationPreference::Ask;
}

[[nodiscard]] constexpr std::string_view
adaptationPreferenceToString(const AdaptationPreference p) noexcept {
    for (const auto &[v, n]: kAdaptationPreferenceEntries) {
        if (v == p) return n;
    }
    return "Ask";
}
