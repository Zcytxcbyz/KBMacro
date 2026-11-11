#pragma once

#include <wx/statusbr.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <wx/string.h>

// status bar.
//
// Single field showing the current status text. Each panel's run status is updated via SetMessage.
// Field splitting and complex status management will follow once ThreadManager is integrated.
class KBMacroStatusBar : public wxStatusBar {
public:
    explicit KBMacroStatusBar(wxWindow *parent);

    // show status text (overwrites current content)
    void SetMessage(const wxString &text);

    // persistent notice on the right (hotkey registration failures, config problems, etc.).
    // independent of the left run status: status is recomputed every 300ms, notice only when needed.
    void SetNotice(const wxString &text);

    // clear status text
    void Clear();
};
