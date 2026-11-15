#pragma once

#include <memory>

#include <wx/scrolwin.h>
#include <wx/wx.h>

#include "common/ITimer.hpp"
#include "common/KeyboardParams.hpp"
#include "common/Types.hpp"

class ThreadManager;

// Keyboard simulator panel.
//
//   - Params are assembled in this panel (collectParams / applyParams), snapshotted to ThreadManager on start
//   - The timer is borrowed from MainFrame (a reference to the shared_ptr itself, so it follows mode switches)
//   - "Send once" is a synchronous one-shot action: no thread, no ThreadManager
class KeyboardPanel : public wxScrolledWindow {
public:
    KeyboardPanel(wxWindow *parent, ThreadManager &threadMgr, std::shared_ptr<ITimer> &timerRef);

    // Called by MainFrame from the hotkey path, equivalent to the user clicking "Start"/"Stop".
    void startWithCurrentParams();
    void stop();

    void refreshHotkeyHints();

    // Called by MainFrame from the hotkey path, equivalent to the user clicking "Send once".
    // No RunSpec / thread involved: calls SendInput once with the current hotkey.
    void sendOnce();

private:
    // Toggle the "running" button state: disable Start and enable Stop while running
    void setRunning(bool running);

    void buildUi();

    // Assemble KeyboardParams from the widgets
    [[nodiscard]] KeyboardParams collectParams() const;

    // Sync KeyboardParams into the widgets
    void applyParams(const KeyboardParams &p);

    void onSetKey(wxCommandEvent &event);

    void onSendOnce(wxCommandEvent &event);

    void onStart(wxCommandEvent &event);

    void onStop(wxCommandEvent &event);

    void onImport(wxCommandEvent &event);

    void onExport(wxCommandEvent &event);

    void refreshEnableStates();
    void onRunModeChanged(wxCommandEvent &event);

    // Refresh the key display field
    void refreshKeyDisplay();

    ThreadManager &m_threadMgr;
    std::shared_ptr<ITimer> &m_timerRef;

    // ---- Key ----
    Hotkey m_hotkey;
    wxTextCtrl *m_keyDisplay{};
    wxButton *m_btnSetKey{};

    // ---- Params ----
    wxSpinCtrl *m_spinInterval{};
    wxSpinCtrl *m_spinHoldTime{};

    // ---- Run mode ----
    wxChoice *m_choiceRunMode{};
    wxSpinCtrl *m_spinDuration{};
    wxSpinCtrl *m_spinCount{};
    wxSpinCtrl *m_spinStartDelay{};

    // ---- Actions ----
    wxButton *m_btnSendOnce{};
    wxButton *m_btnStart{};
    wxButton *m_btnStop{};
    wxButton *m_btnImport{};
    wxButton *m_btnExport{};
};
