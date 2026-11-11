#include "ui/KBMacroStatusBar.hpp"

KBMacroStatusBar::KBMacroStatusBar(wxWindow *parent)
    : wxStatusBar(parent, wxID_ANY) {
    // field 0: run status (flexible); field 1: persistent notice (fixed width)
    SetFieldsCount(2);
    const int widths[2] = {-1, FromDIP(240)};
    SetStatusWidths(2, widths);
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
