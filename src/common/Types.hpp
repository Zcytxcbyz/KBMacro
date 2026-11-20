#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

// ============================================================
// enums and constants shared across modules
// ============================================================

// timer mode: persisted by ConfigManager, consumed by ITimer
enum class TimerMode {
    Sleep,
    Spin,
    Hybrid, // chunked sleep for long waits, busy wait at the tail; spin duration in Config::timerHybridSpinMs
};

struct TimerModeEntry {
    TimerMode mode;
    std::string_view name;
};

inline constexpr TimerModeEntry kTimerModeEntries[] = {
        {.mode = TimerMode::Sleep, .name = "sleep"},
        {.mode = TimerMode::Spin, .name = "spin"},
        {.mode = TimerMode::Hybrid, .name = "hybrid"},
};

[[nodiscard]] constexpr TimerMode timerModeFromString(const std::string_view s) noexcept {
    for (const auto &[mode, name] : kTimerModeEntries) {
        if (name == s) return mode;
    }
    return TimerMode::Sleep;
}

[[nodiscard]] constexpr std::string_view timerModeToString(const TimerMode m) noexcept {
    for (const auto &[mode, name] : kTimerModeEntries) {
        if (mode == m) return name;
    }
    return "sleep";
}

// ============================================================
// hotkeys
// ============================================================

// modifier bitmasks
namespace hk {
constexpr unsigned kModCtrl = 1u << 0;
constexpr unsigned kModShift = 1u << 1;
constexpr unsigned kModAlt = 1u << 2;
constexpr unsigned kModWin = 1u << 3;
} // namespace hk

// one key combination: modifier bitmask + main-key virtual key code
// vk == 0 means "no main key set" (possibly still waiting for the user to press one)
struct Hotkey {
    unsigned modifiers = 0;
    unsigned vk = 0;
};

// ============================================================
// run mode
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
        {.mode = RunMode::Timed, .name = "Timed"},
        {.mode = RunMode::Counted, .name = "Counted"},
};

[[nodiscard]] constexpr RunMode runModeFromString(const std::string_view s) noexcept {
    for (const auto &[mode, name] : kRunModeEntries) {
        if (name == s) return mode;
    }
    return RunMode::Counted;
}

[[nodiscard]] constexpr std::string_view runModeToString(const RunMode m) noexcept {
    for (const auto &[mode, name] : kRunModeEntries) {
        if (mode == m) return name;
    }
    return "Counted";
}

// run mode parameters: infinite / timed / fixed count
struct RunSpec {
    // Counted/1 by default: pressing start should do something finite and obvious,
    // not run until the user notices. Infinite is one click away in the dropdown.
    RunMode mode = RunMode::Counted;
    int durationSec = 10; // only valid when mode == Timed
    int count = 1;        // only valid when mode == Counted
    int startDelayMs = 0; // wait this long after thread start before the first action (independent of mode)
};

// serialization: trimmed by mode
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

    // start delay is independent of mode; omitted when 0 to keep parameter files noise-free
    if (r.startDelayMs > 0) j["startDelayMs"] = r.startDelayMs;
}

inline void from_json(const nlohmann::json &j, RunSpec &r) {
    r = RunSpec{};
    r.mode = runModeFromString(j.value("mode", std::string{"Infinite"}));
    r.durationSec = j.value("durationSec", 10);
    r.count = j.value("count", 100);
    r.startDelayMs = j.value("startDelayMs", 0);
}

// ============================================================
// run finish reason
// ============================================================

// reported when a worker thread exits: finished naturally or stopped by the user.
// never persisted, so no table-driven string conversion.
enum class FinishReason {
    Completed,
    Stopped,
    Failed,
};

// ============================================================
// screen spec
// ============================================================

// records the screen environment at record/export time, used to decide on coordinate adaptation at import.
//   - width / height: primary screen resolution in pixels
//   - dpi: primary screen DPI (96 = 100%, 120 = 125%, 144 = 150%)
// all zero means "not recorded" (old scripts or files not produced by KBMacro).
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
// coordinate adaptation preference
// ============================================================

// preferred handling when an imported script has a different screen spec.
// Ask prompts every time; the other two act directly without prompting.
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
        {.value = AdaptationPreference::Ask, .name = "Ask"},
        {.value = AdaptationPreference::AlwaysAdapt, .name = "AlwaysAdapt"},
        {.value = AdaptationPreference::KeepOriginal, .name = "KeepOriginal"},
};

[[nodiscard]] constexpr AdaptationPreference adaptationPreferenceFromString(const std::string_view s) noexcept {
    for (const auto &[v, n] : kAdaptationPreferenceEntries) {
        if (n == s) return v;
    }
    return AdaptationPreference::Ask;
}

[[nodiscard]] constexpr std::string_view adaptationPreferenceToString(const AdaptationPreference p) noexcept {
    for (const auto &[v, n] : kAdaptationPreferenceEntries) {
        if (v == p) return n;
    }
    return "Ask";
}
