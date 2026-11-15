#pragma once

// ReSharper disable once CppUnusedIncludeDirective
#include <cstddef>
#include <iterator>
#include <string>

#include <wx/button.h>

#include "common/Hotkey.hpp"
#include "common/HotkeyAction.hpp"
// ReSharper disable once CppUnusedIncludeDirective
#include "common/Utils.hpp"
#include "managers/ConfigManager.hpp"
#include "managers/LanguageManager.hpp"
#include "managers/LanguageManager.hpp"
#include "ui/DialogButtons.hpp"

// Buttons that a global hotkey can also trigger carry the key in their label
// ("Start (F1)"), so the user does not have to remember the mapping.
// An unset or disabled hotkey keeps the bare label.
namespace uibtn {

    // " (F1)", or empty when the slot is unset or disabled.
    [[nodiscard]] inline std::wstring hotkeySuffix(const HotkeyAction action) {
        const auto &cfg = ConfigManager::instance().config();
        const auto index = static_cast<std::size_t>(action);
        if (index >= std::size(cfg.hotkeyEnabled) || !cfg.hotkeyEnabled[index]) {
            return {};
        }

        const std::wstring key = utils::hotkeyToString(cfg.hotkeys[index]);
        if (key.empty()) {
            return {};
        }
        return L" (" + key + L")";
    }

    // baseResId is the resource string the button was created with.
    inline void applyHotkeyHint(wxButton *button, const int baseResId, const HotkeyAction action) {
        if (button == nullptr) {
            return;
        }
        button->SetLabel(wxString(TR(baseResId) + hotkeySuffix(action)));
    }

    // ------------------------------------------------------------------
    // "This key is owned by a global hotkey" warning.
    //
    // RegisterHotKey is a system-wide registration and ignores the 'KBMR' marker
    // on injected events, so an injected key fires the hotkey exactly like a real
    // key press. This warns before the user walks into it; the send is never blocked.
    // ------------------------------------------------------------------

    // Display text of the enabled hotkey whose main key is vk; empty when none.
    [[nodiscard]] inline std::wstring hotkeyOwningMainKey(const unsigned vk) {
        if (vk == 0) {
            return {};
        }

        const auto &cfg = ConfigManager::instance().config();
        for (std::size_t i = 0; i < cfg.hotkeys.size(); ++i) {
            if (cfg.hotkeyEnabled[i] && cfg.hotkeys[i].vk == vk) {
                return utils::hotkeyToString(cfg.hotkeys[i]);
            }
        }
        return {};
    }

    // Shows the warning when vk belongs to an enabled hotkey; returns true when shown.
    [[nodiscard]] inline bool warnIfHotkeyKey(wxWindow *parent, const unsigned vk) {
        const std::wstring owner = hotkeyOwningMainKey(vk);
        if (owner.empty()) {
            return false;
        }

        uibtn::messageBox(wxString::Format(wxString(TR(IDS_MSG_KEY_IS_HOTKEY)), owner.c_str()),
                          TR(IDS_APP_TITLE),
                          wxOK | wxICON_WARNING,
                          parent);
        return true;
    }

} // namespace uibtn
