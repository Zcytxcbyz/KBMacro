#pragma once

#include <functional>

#include <wx/wx.h>

#include "common/Types.hpp"

// Hotkey capture dialog.
//
// usage:
//     HotkeyCaptureDialog dlg(this, "Set Key", currentHotkey, false);
//     if (dlg.ShowModal() == wxID_OK) {
//         Hotkey result = dlg.getHotkey();
//     }
//
// requireMainKey == true  requires a main key (for global hotkeys)
// requireMainKey == false allows modifier-only or main-key-only (for keyboard simulation)
//
// checkConflict (optional): external conflict checker. When non-empty, onOk calls it;
//   true means the hotkey conflicts with an existing one; a warning is shown and the dialog stays open.
//   Used by SettingsPanel to detect duplicates when changing a global hotkey.
class HotkeyCaptureDialog : public wxDialog {
public:
    using ConflictChecker = std::function<bool(const Hotkey &)>;

    HotkeyCaptureDialog(wxWindow *parent,
                        const wxString &title,
                        const Hotkey &initial,
                        bool requireMainKey,
                        ConflictChecker checkConflict = {});

    [[nodiscard]] Hotkey getHotkey() const noexcept { return m_hotkey; }

private:
    void onCharHook(const wxKeyEvent &event);
    void onClear(wxCommandEvent &event);
    void onOk(wxCommandEvent &event);

    void updateDisplay() const;

    Hotkey m_hotkey;
    bool m_requireMainKey = false;
    ConflictChecker m_checkConflict;   // empty = no conflict check
    wxTextCtrl *m_display{};
};
