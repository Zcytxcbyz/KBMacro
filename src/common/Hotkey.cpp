#include "common/Hotkey.hpp"

#include <utility>

#include <windows.h>

#include "common/KeyNames.gen.hpp"
#include "common/Utils.hpp"

namespace utils {

    namespace {

        struct ModifierEntry {
            const wchar_t *name;
            unsigned bit;
        };

        constexpr ModifierEntry kModifiers[] = {
            {.name = L"Ctrl",  .bit = hk::kModCtrl},
            {.name = L"Shift", .bit = hk::kModShift},
            {.name = L"Alt",   .bit = hk::kModAlt},
            {.name = L"Win",   .bit = hk::kModWin},
        };

        unsigned modifierBit(const std::wstring_view name) {
            for (const auto &[n, b]: kModifiers) {
                if (name == n) return b;
            }
            return 0;
        }

    } // namespace

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
        for (const auto &[v, name]: kKeyNames) {
            if (v == vk) return name;
        }
        return L"?";
    }

    unsigned vkFromString(const std::wstring_view name) {
        if (name.empty()) return 0;
        for (const auto &[vk, n]: kKeyNames) {
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

        for (const auto &[name, bit]: kModifiers) {
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

    void to_json(nlohmann::json &j, const Hotkey &hk) {
        j = nlohmann::json::object();

        nlohmann::json mods = nlohmann::json::array();
        for (const auto &[name, bit]: kModifiers) {
            if (hk.modifiers & bit) {
                mods.push_back(ToUtf8(name));
            }
        }
        j["modifiers"] = std::move(mods);

        j["vk"] = ToUtf8(vkToString(hk.vk));
    }

    void from_json(const nlohmann::json &j, Hotkey &hk) {
        hk = Hotkey{};

        if (j.contains("modifiers") && j["modifiers"].is_array()) {
            for (const auto &item: j["modifiers"]) {
                if (!item.is_string()) continue;
                const std::wstring wname = ToWide(item.get<std::string>());
                hk.modifiers |= modifierBit(wname);
            }
        }

        if (j.contains("vk") && j["vk"].is_string()) {
            const std::string utf8 = j["vk"].get<std::string>();
            if (!utf8.empty()) {
                const std::wstring wname = ToWide(utf8);
                hk.vk = vkFromString(wname);
            }
        }
    }

} // namespace utils
