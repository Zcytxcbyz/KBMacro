#pragma once

// ReSharper disable once CppUnusedIncludeDirective
#include <cstddef>
#include <iterator>
#include <string>

#include <wx/button.h>

#include "common/Hotkey.hpp"
#include "common/HotkeyAction.hpp"
#include "common/Utils.hpp"
#include "managers/ConfigManager.hpp"
#include "managers/LanguageManager.hpp"

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

} // namespace uibtn
