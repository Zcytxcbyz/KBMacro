#pragma once

#include <iterator>

// ReSharper disable once CppUnusedIncludeDirective
#include "resource.h"

// ============================================================
// global hotkey actions
//
// the enum value is the hotkey array index and maps 1:1 to Config::hotkeys.
// reordering breaks saved config files, so the order must not change.
// ============================================================

enum class HotkeyAction {
    StartClicker,
    StopClicker,
    StartKeyboard,
    StopKeyboard,
    SendOnce,
    StartRecord,
    StopRecord,
    StartPlayback,
    StopPlayback,
    StartAll,
    StopAll,
};

// number of hotkeys. shared by Config / HotkeyManager / SettingsPanel.
inline constexpr int kHotkeyCount = 11;

// action → display-name resource id. SettingsPanel indexes into it directly.
inline constexpr int kHotkeyActionResIds[kHotkeyCount] = {
    IDS_SETTINGS_HOTKEY_START_CLICKER,
    IDS_SETTINGS_HOTKEY_STOP_CLICKER,
    IDS_SETTINGS_HOTKEY_START_KEYBOARD,
    IDS_SETTINGS_HOTKEY_STOP_KEYBOARD,
    IDS_SETTINGS_HOTKEY_SEND_ONCE,
    IDS_SETTINGS_HOTKEY_START_RECORD,
    IDS_SETTINGS_HOTKEY_STOP_RECORD,
    IDS_SETTINGS_HOTKEY_START_PLAYBACK,
    IDS_SETTINGS_HOTKEY_STOP_PLAYBACK,
    IDS_SETTINGS_HOTKEY_START_ALL,
    IDS_SETTINGS_HOTKEY_STOP_ALL,
};

static_assert(std::size(kHotkeyActionResIds) == kHotkeyCount,
              "The hotkey action list doesn't match kHotkeyCount");
