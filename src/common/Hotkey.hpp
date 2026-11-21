#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "common/Types.hpp"

namespace utils {

// Hotkey -> display string, e.g. "Ctrl+Alt+F1"
// modifier order is fixed: Ctrl, Shift, Alt, Win
// an empty hotkey returns an empty string; modifiers only (vk == 0) return e.g. "Ctrl"
[[nodiscard]] std::wstring hotkeyToString(const Hotkey &hk);

// virtual key code -> key name, e.g. VK_F1 -> "F1"
// unknown keys return "?", vk == 0 returns an empty string
[[nodiscard]] std::wstring vkToString(unsigned vk);

// key name -> virtual key code, e.g. "F1" -> VK_F1
// returns 0 when not found
[[nodiscard]] unsigned vkFromString(std::wstring_view name);

// validate a hotkey
//   requireMainKey == true  a main key is mandatory (vk != 0), used for global hotkeys
//   requireMainKey == false modifiers only or a main key only are allowed
// no modifiers and no main key -> false
[[nodiscard]] bool isValidHotkey(const Hotkey &hk, bool requireMainKey);

// check whether a virtual key code is a modifier key itself
[[nodiscard]] bool isModifierKey(unsigned vk);

// modifier virtual key -> hk::kMod* bitmask; non-modifiers return 0
[[nodiscard]] unsigned modifierBitForVk(unsigned vk);

} // namespace utils

// ============================================================
// nlohmann ADL serialization for Hotkey
//
// JSON shape:
//   { "modifiers": ["Ctrl", "Shift"], "vk": "F1" }
//
// modifiers holds an array of modifier names (fixed order Ctrl/Shift/Alt/Win)
// vk holds the key name string, an empty string when unset
//
// placed in the global namespace next to Hotkey so ADL can find it.
// ============================================================

void to_json(nlohmann::json &j, const Hotkey &hk);
void from_json(const nlohmann::json &j, Hotkey &hk);
