#pragma once

#include <wx/wx.h>
#include <wx/scrolwin.h>

#include "common/ClickerParams.hpp"

class ThreadManager;

// Mouse auto-clicker panel.
//
//   - Params are assembled in this panel (collectParams / applyParams), snapshotted to ThreadManager on start
//   - Actual run state comes from ThreadManager's Slot; the panel only toggles buttons accordingly
class ClickerPanel : public wxScrolledWindow {
public:
    ClickerPanel(wxWindow *parent, ThreadManager &threadMgr);

    // Called by MainFrame from the hotkey path, equivalent to the user clicking "Start"/"Stop".
    // Assemble ClickerParams from the current widget values and hand them to ThreadManager to start.
    // Real implementation waits for the Clicker module; currently only logs as a placeholder.
    void startWithCurrentParams();
    void stop();

private:
    // Toggle the "running" button state: disable Start and enable Stop while running
    void setRunning(bool running);

    void buildUi();

    // Assemble ClickerParams from the widgets
    [[nodiscard]] ClickerParams collectParams() const;

    // Sync ClickerParams into the widgets; calls refreshEnableStates at the end
    void applyParams(const ClickerParams &p);

    void onCapturePos(wxCommandEvent &event);

    void onIgnorePos(wxCommandEvent &event);

    void onStart(wxCommandEvent &event);

    void onStop(wxCommandEvent &event);

    void onImport(wxCommandEvent &event);

    void onExport(wxCommandEvent &event);

    void refreshEnableStates();
    void onTypeChanged(wxCommandEvent &event);
    void onRunModeChanged(wxCommandEvent &event);
    void onCoordChar(wxKeyEvent &event);
    [[nodiscard]] bool isWheelTypeSelected() const;

    ThreadManager &m_threadMgr;

    // ---- Coordinates ----
    wxTextCtrl *m_x{};
    wxTextCtrl *m_y{};
    wxButton *m_btnCapture{};
    wxCheckBox *m_chkIgnorePos{};

    // ---- Click settings ----
    wxChoice *m_choiceType{};
    wxSpinCtrl *m_spinWheelSteps{};
    wxSpinCtrl *m_spinInterval{};
    wxSpinCtrl *m_spinHoldTime{};

    // ---- Run mode ----
    wxChoice *m_choiceRunMode{};
    wxSpinCtrl *m_spinDuration{};
    wxSpinCtrl *m_spinCount{};
    wxSpinCtrl *m_spinStartDelay{};

    // ---- Actions ----
    wxButton *m_btnStart{};
    wxButton *m_btnStop{};
    wxButton *m_btnImport{};
    wxButton *m_btnExport{};
};
