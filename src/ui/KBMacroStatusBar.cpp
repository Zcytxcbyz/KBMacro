#include "ui/KBMacroStatusBar.hpp"

KBMacroStatusBar::KBMacroStatusBar(wxWindow *parent)
    : wxStatusBar(parent, wxID_ANY) {
    // 字段 0：运行状态（弹性）；字段 1：常驻提示（固定宽度）
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
