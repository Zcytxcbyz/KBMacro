#pragma once

#include <wx/wx.h>
#include <wx/hyperlink.h>
#include <wx/scrolwin.h>
#include <wx/listctrl.h>

// About panel.
//
// Shows app info, the third-party library list, and the full license texts (including KBMacro itself).
// License texts are loaded from RC RCDATA resources and concatenated into a read-only multiline text box.
class AboutPanel : public wxScrolledWindow {
public:
    explicit AboutPanel(wxWindow *parent);

private:
    void buildUi();
    void onShowLicense(wxCommandEvent &event);

    wxListCtrl *m_libList{};
    wxButton *m_btnLicense{};
};
