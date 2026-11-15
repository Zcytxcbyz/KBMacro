#pragma once

#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/window.h>

#include "managers/LanguageManager.hpp"
#include "resource.h"

// ============================================================
// Standard button label localization
//
// Labels of the wxID_OK / wxID_CANCEL / wxID_YES / wxID_NO standard buttons come from
// wxWidgets' bundled wxstd.mo translation files. This project is a single-EXE portable
// build without wx language files, so these buttons stay English on the Chinese UI.
// Here we replace their labels with resource strings by id once they are created.
//
// usage: after CreateStdDialogButtonSizer() / CreateButtonSizer() and before
//       SetSizerAndFit() / SetSizer(), call uibtn::localize(this).
// ============================================================
namespace uibtn {

// ReSharper disable once CppParameterMayBeConstPtrOrRef
inline void localize(wxWindow *owner) {
    constexpr struct {
        int id;
        int resId;
    } kMap[] = {
            {wxID_OK, IDS_COMMON_BTN_OK},
            {wxID_CANCEL, IDS_COMMON_BTN_CANCEL},
            {wxID_YES, IDS_COMMON_BTN_YES},
            {wxID_NO, IDS_COMMON_BTN_NO},
    };

    for (const auto &entry : kMap) {
        if (auto *btn =
                    wxDynamicCast(owner->FindWindow(entry.id), wxButton)) { // NOLINT(*-pro-type-static-cast-downcast)
            btn->SetLabel(TR(entry.resId));
        }
    }
}

// ---- message box label localization ----
//
// wxMessageBox's standard buttons also come from wxstd.mo and show English on the Chinese UI.
// Here we set the labels manually via wxMessageDialog; its return value matches wxMessageBox
// (wxID_OK / wxID_YES / wxID_CANCEL ...).
//
// The parameter order intentionally matches wxMessageBox (text, caption, style, parent),
// so call sites only need to replace wxMessageBox( with uibtn::messageBox(.
[[nodiscard]] inline wxMessageDialogBase::ButtonLabel label(const int resId) {
    return {wxMessageDialogBase::ButtonLabel(wxString(TR(resId)))};
}

inline int messageBox(const wxString &text,
                      const wxString &caption,
                      const long style = wxOK | wxICON_INFORMATION,
                      wxWindow *parent = nullptr) {
    wxMessageDialog dlg(parent, text, caption, style);

    if ((style & wxYES_NO) != 0) {
        if ((style & wxCANCEL) != 0) {
            dlg.SetYesNoCancelLabels(label(IDS_COMMON_BTN_YES), label(IDS_COMMON_BTN_NO), label(IDS_COMMON_BTN_CANCEL));
        } else {
            dlg.SetYesNoLabels(label(IDS_COMMON_BTN_YES), label(IDS_COMMON_BTN_NO));
        }
    } else {
        dlg.SetOKLabel(label(IDS_COMMON_BTN_OK));
    }

    return dlg.ShowModal();
}

} // namespace uibtn
