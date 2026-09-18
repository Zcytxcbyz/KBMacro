#include "ui/KBMacroStatusBar.hpp"

KBMacroStatusBar::KBMacroStatusBar(wxWindow *parent)
    : wxStatusBar(parent, wxID_ANY) {
}

void KBMacroStatusBar::SetMessage(const wxString &text) {
    SetStatusText(text, 0);
}

void KBMacroStatusBar::Clear() {
    SetStatusText(wxEmptyString, 0);
}
