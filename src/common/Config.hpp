#pragma once

#include <array>
#include <nlohmann/json.hpp>
#include <string>

#include "common/HotkeyAction.hpp"
#include "common/Types.hpp"

// ============================================================
// global configuration data
//
// only fields that need persistence (global settings: language, timer, always-on-top, hotkeys, flags).
//
// not here:
//   - per-module parameters (ClickerParams / KeyboardParams / recording scripts)
//     → persisted separately by each module's "import / export parameters" buttons
//   - runtime state (threads, timer instances, UI widget values)
// ============================================================

// schema version of the config JSON. bumped by 1 on incompatible structure changes.
// on load, a file version > kConfigVersion means a newer file, which must be rejected.
inline constexpr int kConfigVersion = 1;

// default of the hotkey-enabled flags: all true.
// old config files without hotkeyEnabled get this default,
// so configured hotkeys are never silently disabled after an upgrade.
inline constexpr auto kHotkeyEnabledDefault = [] {
    std::array<bool, kHotkeyCount> a{};
    a.fill(true);
    return a;
}();

// default hotkeys: single keys F1–F11, 1:1 with the HotkeyAction order
//   F1 start clicker / F2 stop clicker / F3 start keyboard / F4 stop keyboard / F5 send once
//   F6 start recording / F7 stop recording / F8 start playback / F9 stop playback
//   F10 start all / F11 stop all
//
// vk uses raw numbers because Config.hpp does not include windows.h (VK_F1 == 0x70 … VK_F11 == 0x7A).
inline constexpr std::array<Hotkey, kHotkeyCount> kHotkeyDefault{{
        {.modifiers = 0, .vk = 0x70},
        {.modifiers = 0, .vk = 0x71},
        {.modifiers = 0, .vk = 0x72},
        {.modifiers = 0, .vk = 0x73},
        {.modifiers = 0, .vk = 0x74},
        {.modifiers = 0, .vk = 0x75},
        {.modifiers = 0, .vk = 0x76},
        {.modifiers = 0, .vk = 0x77},
        {.modifiers = 0, .vk = 0x78},
        {.modifiers = 0, .vk = 0x79},
        {.modifiers = 0, .vk = 0x7A},
}};

// range and default of the hybrid timer's "tail spin duration" (ms).
// defined here once so the settings page and every validation share one copy, no magic numbers.
inline constexpr int kTimerHybridSpinMsMin = 1;
inline constexpr int kTimerHybridSpinMsMax = 50;
inline constexpr int kTimerHybridSpinMsDefault = 10;

// ranges and defaults of the recording settings.
//   recordMoveIntervalMs — WH_MOUSE_LL move sampling interval: moves closer than that are dropped.
//                          0 means no sampling (record all), 1000 means at most one per second.
//   recordStartDelayMs   — wait this long after starting to record, dodging the start hotkey itself.
//   recordMaxActions     — action limit per recording, auto-stops when reached. 0 = unlimited.
inline constexpr int kRecordMoveIntervalMsMin = 0;
inline constexpr int kRecordMoveIntervalMsMax = 1000;
inline constexpr int kRecordMoveIntervalMsDefault = 16;

inline constexpr int kRecordStartDelayMsMin = 0;
inline constexpr int kRecordStartDelayMsMax = 2000;
inline constexpr int kRecordStartDelayMsDefault = 0;

inline constexpr int kRecordMaxActionsMin = 0;
inline constexpr int kRecordMaxActionsMax = 1000000;
inline constexpr int kRecordMaxActionsDefault = 100000;

// playback factor: only these steps are allowed, the UI drop-down and the config file share one table
inline constexpr double kPlaybackSpeeds[] = {0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0};
inline constexpr double kPlaybackSpeedDefault = 1.0;

// snap any value to the nearest legal step; <= 0 or NaN falls back to the default.
// the config file is hand-edited, so both read and write paths run it through this function.
[[nodiscard]] double snapPlaybackSpeed(double speed) noexcept;

struct Config {
    // UI language code ("zh-CN" / "en-US").
    // empty means "never chosen by the user"; startup falls back to the system language, no write-back.
    std::string language;

    TimerMode timerMode = TimerMode::Sleep;

    // spin-tail milliseconds of the hybrid timer (TimerMode::Hybrid), range in
    // kTimerHybridSpinMsMin/Max. The other two modes do not use it.
    int timerHybridSpinMs = kTimerHybridSpinMsDefault;

    // ---- recording settings (applied when the next recording starts) ----

    // mouse move sampling interval (ms), range in kRecordMoveIntervalMsMin/Max
    int recordMoveIntervalMs = kRecordMoveIntervalMsDefault;

    // whether events injected by other programs (AutoHotkey, IMEs, etc.) are skipped too.
    // events injected by this program (dwExtraInfo == 'KBMR') are never recorded, whatever this switch says.
    bool recordIgnoreInjected = false;

    // wait before recording starts (ms), filters out the start hotkey's own keystrokes
    int recordStartDelayMs = kRecordStartDelayMsDefault;

    // action limit per recording, 0 = unlimited
    int recordMaxActions = kRecordMaxActionsDefault;

    // playback factor (2.0 = double speed). only scales delayMs/holdMs before playback,
    // not written into *.macro.json
    double playbackSpeed = kPlaybackSpeedDefault;

    bool alwaysOnTop = false;

    // last selected tab index. range check lives in MainFrame (only it knows the tab count).
    int tabIndex = 0;

    // global hotkeys, order matches HotkeyManager's action table.
    std::array<Hotkey, kHotkeyCount> hotkeys = kHotkeyDefault;

    // enable switches for each hotkey, 1:1 with hotkeys.
    // disabled hotkeys are not registered and skip conflict checks; all enabled by default.
    std::array<bool, kHotkeyCount> hotkeyEnabled = kHotkeyEnabledDefault;

    // main window geometry.
    // x/y == -1 means "not set", startup uses wxDefaultPosition.
    // MainFrame also checks the position against the virtual desktop; off-screen positions are ignored.
    struct Window {
        int x = -1;
        int y = -1;
        int width = 800;
        int height = 850;
        bool maximized = false;
    } window;

    // misc boolean flags, kept together so the top level does not grow.
    struct Flags {
        // user has acknowledged the "high-precision Spin raises CPU usage" warning
        bool spinWarningConfirmed = false;
        // coordinate adaptation preference when importing recording scripts
        AdaptationPreference adaptationPreference = AdaptationPreference::Ask;
    } flags;
};

// ============================================================
// nlohmann ADL serialization
//
// JSON shape:
//   {
//     "version": 1,
//     "language": "en-US",
//     "timerMode": "sleep",
//     "timerHybridSpinMs": 10,
//     "recordMoveIntervalMs": 16,
//     "recordIgnoreInjected": false,
//     "recordStartDelayMs": 200,
//     "recordMaxActions": 100000,
//     "playbackSpeed": 1.0,
//     "alwaysOnTop": false,
//     "tabIndex": 0,
//     "hotkeys": [ {...}, ... ],
//     "hotkeyEnabled": [ true, false, ... ],
//     "window": { "x":..., "y":..., "width":..., "height":..., "maximized":... },
//     "flags": { "spinWarningConfirmed": false, "adaptationPreference": "Ask" }
//   }
//
// compatibility policy:
//   - missing fields → defaults
//   - extra fields → ignored
//   - "version" is validated by ConfigPaths in loadFrom and never reaches the Config struct
// ============================================================

void to_json(nlohmann::json &j, const Config &c);
void from_json(const nlohmann::json &j, Config &c);
