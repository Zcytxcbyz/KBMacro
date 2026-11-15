#include "ui/dialogs/HotkeyCaptureDialog.hpp"

// ReSharper disable once CppUnusedIncludeDirective
#include <utility>
// ReSharper disable once CppUnusedIncludeDirective
#include <wx/statbox.h>

#include "common/Hotkey.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp" // ToUtf8
#include "managers/LanguageManager.hpp"
#include "ui/DialogButtons.hpp"

HotkeyCaptureDialog::HotkeyCaptureDialog(wxWindow *parent,
                                         const wxString &title,
                                         const Hotkey &initial,
                                         const bool requireMainKey,
                                         ConflictChecker checkConflict)
    : wxDialog(parent, wxID_ANY, title),
      m_hotkey(initial),
      m_requireMainKey(requireMainKey),
      m_checkConflict(std::move(checkConflict)) {
    auto *root = new wxBoxSizer(wxVERTICAL);

    // ---- capture display ----
    m_display = new wxTextCtrl(
            this, wxID_ANY, "", wxDefaultPosition, FromDIP(wxSize(360, 32)), wxTE_READONLY | wxTE_CENTRE);
    root->Add(m_display, 0, wxEXPAND | wxALL, 12);

    // ---- button row: clear ----
    auto *clearRow = new wxBoxSizer(wxHORIZONTAL);
    auto *btnClear = new wxButton(this, wxID_ANY, TR(IDS_HOTKEY_DIALOG_BTN_CLEAR));
    clearRow->Add(btnClear, 0, 0);
    clearRow->AddStretchSpacer(1);
    root->Add(clearRow, 0, wxEXPAND | wxLEFT | wxRIGHT, 12);

    // ---- standard button row: OK/cancel ----
    auto *btnSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    root->Add(btnSizer, 0, wxEXPAND | wxALL, 12);

    uibtn::localize(this); // swap standard button labels for localized ones (wx ships no language files)

    SetSizerAndFit(root);
    wxTopLevelWindowBase::SetMinSize(wxSize(FromDIP(400), -1));

    // ---- focus & keyboard capture ----
    // wxEVT_CHAR_HOOK fires before keyboard input reaches child controls, so it intercepts all keys
    Bind(wxEVT_CHAR_HOOK, &HotkeyCaptureDialog::onCharHook, this);
    btnClear->Bind(wxEVT_BUTTON, &HotkeyCaptureDialog::onClear, this);
    Bind(wxEVT_BUTTON, &HotkeyCaptureDialog::onOk, this, wxID_OK);

    updateDisplay();
    m_display->SetFocus();
}

// ============================================================
// keyboard capture
// ============================================================
void HotkeyCaptureDialog::onCharHook(const wxKeyEvent &event) {
    const unsigned rawVk = event.GetRawKeyCode();

    // Esc: clear the current capture without closing the dialog
    if (rawVk == VK_ESCAPE) {
        m_hotkey = Hotkey{};
        updateDisplay();
        return;
    }

    // merge modifiers currently held (accumulate only, never clear)
    const int wxMods = event.GetModifiers();
    if (wxMods & wxMOD_CONTROL) m_hotkey.modifiers |= hk::kModCtrl;
    if (wxMods & wxMOD_SHIFT) m_hotkey.modifiers |= hk::kModShift;
    if (wxMods & wxMOD_ALT) m_hotkey.modifiers |= hk::kModAlt;
    if (wxMods & wxMOD_WIN) m_hotkey.modifiers |= hk::kModWin;

    // main key (non-modifier): update vk
    if (!utils::isModifierKey(rawVk)) {
        m_hotkey.vk = rawVk;
    }

    LOG_DEBUG("hotkey dialog: captured '{}'", utils::ToUtf8(utils::hotkeyToString(m_hotkey)));
    updateDisplay();
    // no event.Skip(): keep the key from propagating to child controls
}

// ============================================================
// clear / OK
// ============================================================
void HotkeyCaptureDialog::onClear(wxCommandEvent &) {
    m_hotkey = Hotkey{};
    updateDisplay();
    m_display->SetFocus();
}

void HotkeyCaptureDialog::onOk(wxCommandEvent &event) {
    if (!utils::isValidHotkey(m_hotkey, m_requireMainKey)) {
        uibtn::messageBox(TR(IDS_HOTKEY_DIALOG_MSG_INVALID), GetTitle(), wxOK | wxICON_WARNING, this);
        return;
    }

    // conflict check: an empty hotkey (cleared) is skipped
    const bool isEmpty = (m_hotkey.vk == 0 && m_hotkey.modifiers == 0);
    if (!isEmpty && m_checkConflict && m_checkConflict(m_hotkey)) {
        uibtn::messageBox(TR(IDS_HOTKEY_DIALOG_MSG_CONFLICT), GetTitle(), wxOK | wxICON_WARNING, this);
        return;
    }

    event.Skip();
}

// ============================================================
// refresh display
// ============================================================
void HotkeyCaptureDialog::updateDisplay() const {
    if (const std::wstring s = utils::hotkeyToString(m_hotkey); s.empty()) {
        m_display->SetValue(TR(IDS_HOTKEY_DIALOG_HINT));
    } else {
        m_display->SetValue(s);
    }
}
