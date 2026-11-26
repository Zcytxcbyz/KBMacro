#include "common/Hotkey.hpp"

#include <utility>

#include <windows.h>

#include "common/KeyNames.gen.hpp"
#include "common/Utils.hpp"

namespace {

struct ModifierEntry {
    const wchar_t *name;
    unsigned bit;
};

constexpr ModifierEntry kModifiers[] = {
        {.name = L"Ctrl", .bit = hk::kModCtrl},
        {.name = L"Shift", .bit = hk::kModShift},
        {.name = L"Alt", .bit = hk::kModAlt},
        {.name = L"Win", .bit = hk::kModWin},
};

// Modifier keys are missing from the generated table: GetKeyNameTextW returns an
// empty string for them, so the generator has nothing to record. They still need a
// name in both directions, otherwise a recorded Ctrl/Alt/Shift action serializes as
// "?" and comes back as key 0.
constexpr KeyNameEntry kModifierNames[] = {
    {VK_CONTROL, L"Ctrl"}, {VK_LCONTROL, L"Left Ctrl"}, {VK_RCONTROL, L"Right Ctrl"},
    {VK_SHIFT, L"Shift"},  {VK_LSHIFT, L"Left Shift"},  {VK_RSHIFT, L"Right Shift"},
    {VK_MENU, L"Alt"},     {VK_LMENU, L"Left Alt"},     {VK_RMENU, L"Right Alt"},
    {VK_LWIN, L"Win"},     {VK_RWIN, L"Right Win"},     {VK_APPS, L"Menu"},
};

unsigned modifierBit(const std::wstring_view name) {
    for (const auto &[n, b] : kModifiers) {
        if (name == n) return b;
    }
    return 0;
}

} // namespace

namespace utils {

bool isModifierKey(const unsigned vk) {
    switch (vk) {
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return true;
        default:
            return false;
    }
}

unsigned modifierBitForVk(const unsigned vk) {
    switch (vk) {
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
            return hk::kModCtrl;
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
            return hk::kModShift;
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
            return hk::kModAlt;
        case VK_LWIN:
        case VK_RWIN:
            return hk::kModWin;
        default:
            return 0;
    }
}

std::wstring vkToString(const unsigned vk) {
    if (vk == 0) return {};
    for (const auto &[v, name] : kModifierNames) {
        if (v == vk) return name;
    }
    for (const auto &[v, name] : kKeyNames) {
        if (v == vk) return name;
    }
    return L"?";
}

unsigned vkFromString(const std::wstring_view name) {
    if (name.empty()) return 0;
    for (const auto &[vk, n] : kModifierNames) {
        if (name == n) return vk;
    }
    for (const auto &[vk, n] : kKeyNames) {
        if (name == n) return vk;
    }
    return 0;
}

std::wstring hotkeyToString(const Hotkey &hk) {
    std::wstring result;

    const auto append = [&result](const std::wstring_view s) {
        if (!result.empty()) result += L'+';
        result += s;
    };

    for (const auto &[name, bit] : kModifiers) {
        if (hk.modifiers & bit) append(name);
    }

    if (hk.vk != 0) {
        append(vkToString(hk.vk));
    }

    return result;
}

bool isValidHotkey(const Hotkey &hk, const bool requireMainKey) {
    if (hk.modifiers == 0 && hk.vk == 0) return false;
    if (requireMainKey && hk.vk == 0) return false;
    return true;
}

} // namespace utils

// ============================================================
// Hotkey serialization (global namespace)
// ============================================================

void to_json(nlohmann::json &j, const Hotkey &hk) {
    j = nlohmann::json::object();

    nlohmann::json mods = nlohmann::json::array();
    for (const auto &[name, bit] : kModifiers) {
        if (hk.modifiers & bit) {
            mods.push_back(utils::ToUtf8(name));
        }
    }
    j["modifiers"] = std::move(mods);

    j["vk"] = utils::ToUtf8(utils::vkToString(hk.vk));
}

void from_json(const nlohmann::json &j, Hotkey &hk) {
    hk = Hotkey{};

    if (j.contains("modifiers") && j["modifiers"].is_array()) {
        for (const auto &item : j["modifiers"]) {
            if (!item.is_string()) continue;
            const std::wstring wname = utils::ToWide(item.get<std::string>());
            hk.modifiers |= modifierBit(wname);
        }
    }

    if (j.contains("vk") && j["vk"].is_string()) {
        const std::string utf8 = j["vk"].get<std::string>();
        if (!utf8.empty()) {
            const std::wstring wname = utils::ToWide(utf8);
            hk.vk = utils::vkFromString(wname);
        }
    }
}
