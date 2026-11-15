#include "ui/KBMacroStatusBar.hpp"

KBMacroStatusBar::KBMacroStatusBar(wxWindow *parent)
    : wxStatusBar(parent, wxID_ANY) {
    // field 0: run status (flexible); field 1: persistent notice (fixed width)
    wxStatusBar::SetFieldsCount(2);
    // Proportional fields: the left one shows short run-state text, the right one
    // must fit a full failure notice in every language (ru-RU is the widest).
    constexpr int widths[2] = {-3, -2};
    wxStatusBar::SetStatusWidths(2, widths);
}

void KBMacroStatusBar::SetMessage(const wxString &text) {
    SetStatusText(text, 0);
}

void KBMacroStatusBar::Clear() {
    SetStatusText(wxEmptyString, 0);
}

void KBMacroStatusBar::SetNotice(const wxString &text) {
    SetStatusText(text, 1);
}
