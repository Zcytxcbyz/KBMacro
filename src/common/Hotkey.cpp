#include "common/Hotkey.hpp"

#include <windows.h>

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

        if (vk >= 'A' && vk <= 'Z') {
            return {static_cast<wchar_t>(vk)};
        }
        if (vk >= '0' && vk <= '9') {
            return {static_cast<wchar_t>(vk)};
        }
        if (vk >= VK_F1 && vk <= VK_F24) {
            return L"F" + std::to_wstring(vk - VK_F1 + 1);
        }

        switch (vk) {
            case VK_SPACE:      return L"Space";
            case VK_RETURN:     return L"Enter";
            case VK_TAB:        return L"Tab";
            case VK_ESCAPE:     return L"Esc";
            case VK_BACK:       return L"Backspace";
            case VK_DELETE:     return L"Delete";
            case VK_INSERT:     return L"Insert";
            case VK_HOME:       return L"Home";
            case VK_END:        return L"End";
            case VK_PRIOR:      return L"PageUp";
            case VK_NEXT:       return L"PageDown";
            case VK_LEFT:       return L"Left";
            case VK_RIGHT:      return L"Right";
            case VK_UP:         return L"Up";
            case VK_DOWN:       return L"Down";
            case VK_OEM_PLUS:   return L"+";
            case VK_OEM_MINUS:  return L"-";
            case VK_OEM_COMMA:  return L",";
            case VK_OEM_PERIOD: return L".";
            case VK_OEM_1:      return L";";
            case VK_OEM_2:      return L"/";
            case VK_OEM_3:      return L"`";
            case VK_OEM_4:      return L"[";
            case VK_OEM_5:      return L"\\";
            case VK_OEM_6:      return L"]";
            case VK_OEM_7:      return L"'";
            default:            return L"?";
        }
    }

    std::wstring hotkeyToString(const Hotkey &hk) {
        std::wstring result;

        const auto append = [&result](const std::wstring_view s) {
            if (!result.empty()) result += L'+';
            result += s;
        };

        if (hk.modifiers & hk::kModCtrl)  append(L"Ctrl");
        if (hk.modifiers & hk::kModShift) append(L"Shift");
        if (hk.modifiers & hk::kModAlt)   append(L"Alt");
        if (hk.modifiers & hk::kModWin)   append(L"Win");

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
