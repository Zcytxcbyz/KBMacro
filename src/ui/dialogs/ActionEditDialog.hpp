#pragma once

#include <wx/propgrid/propgrid.h>
#include <wx/wx.h>

#include "common/Action.hpp"

// Action edit dialog.
//
// usage:
//     ActionEditDialog dlg(this, TR(IDS_ACTION_DIALOG_TITLE_ADD), Action{});
//     if (dlg.ShowModal() == wxID_OK) {
//         Action result = dlg.getAction();
//     }
//
// The "add" and "edit" modes share this dialog; the caller decides the title.
//
// interaction:
//   - the top wxChoice selects the action type; switching it rebuilds the parameter table below
//   - parameters are shown in an editable wxPropertyGrid
//   - aux buttons "capture coordinate" / "set key" are enabled by the current action type
class ActionEditDialog : public wxDialog {
public:
    ActionEditDialog(wxWindow *parent, const wxString &title, const Action &action);

    [[nodiscard]] const Action &getAction() const noexcept { return m_action; }

private:
    void buildUi();

    void onTypeChanged(wxCommandEvent &event);

    void onCapturePosition(wxCommandEvent &event);

    void onSetKey(wxCommandEvent &event);

    void onOk(wxCommandEvent &event);

    void onRelativeToggled(wxCommandEvent &event);

    // rebuild the parameter grid from m_action.type
    void rebuildParamGrid();

    // read all fields from the grid back into m_action
    void collectFromGrid();

    // refresh the enabled state of the aux buttons
    void refreshAuxButtons();

    Action m_action;
    wxChoice *m_choiceType{};
    wxCheckBox *m_chkRelative{};
    wxPropertyGrid *m_grid{};
    wxButton *m_btnCapturePos{};
    wxButton *m_btnSetKey{};
};
