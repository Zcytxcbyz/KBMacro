#pragma once

#include <vector>

#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <wx/listctrl.h>

#include "common/Action.hpp"
#include "common/Types.hpp"

#include "managers/ThreadManager.hpp"

// Recording and orchestration panel.
//
// Currently in a transition period: the panel holds std::vector<Action> itself as the data source;
// once the Recorder module lands, the data moves into the module and this panel reads/writes through the interface.
//
// At this stage:
//   - exposes public methods (for the hotkey path)
//   - supports macro script import / export (*.macro.json, whole-list replacement semantics)
class RecorderPanel : public wxScrolledWindow {
public:
    RecorderPanel(wxWindow *parent, ThreadManager &threadMgr);

    // Called by MainFrame from the hotkey path, equivalent to the user clicking the corresponding button.
    // Recording and playback are mutually exclusive, enforced inside ThreadManager; the panel only issues requests.
    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();

private:
    void buildUi();

    // Assemble RunSpec from the widgets (for playback)
    [[nodiscard]] RunSpec collectRunSpec() const;

    // Sync RunSpec into the widgets (for script import)
    void applyRunSpec(const RunSpec &run);

    // Ask the user how to handle coordinates when the screen spec differs
    enum class AdaptChoice {
        Adapt,
        KeepOriginal,
        Cancelled,
    };

    // Pop up a dialog asking whether to adapt coordinates. Checks the adaptationPreference memory first.
    [[nodiscard]] AdaptChoice askAdaptCoordinates(
        const ScreenSpec &scriptSpec, const ScreenSpec &currentSpec);

    // ---- Recording / playback / import-export ----
    void onStartRecord(wxCommandEvent &event);
    void onStopRecord(wxCommandEvent &event);
    void onStartPlayback(wxCommandEvent &event);
    void onStopPlayback(wxCommandEvent &event);
    void onImportScript(wxCommandEvent &event);
    void onExportScript(wxCommandEvent &event);

    // ---- Action list operations ----
    void onAdd(wxCommandEvent &event);
    void onEdit(wxCommandEvent &event);
    void onDelete(wxCommandEvent &event);
    void onMoveUp(wxCommandEvent &event);
    void onMoveDown(wxCommandEvent &event);
    void onClear(wxCommandEvent &event);

    void onListSelectionChanged(wxListEvent &event);

    // ---- Run mode ----
    void onRunModeChanged(wxCommandEvent &event);
    // Sync the playback speed combo box from config (called when the page is shown)
    void syncSpeedFromConfig();
    void onShow(wxShowEvent &event);
    // Playback speed changed: normalize to a step and persist immediately
    void onSpeedChanged(wxCommandEvent &event);
    void refreshEnableStates();

    // ---- List maintenance ----
    void refreshList();
    void refreshButtonStates();
    void selectRow(long row);
    [[nodiscard]] long getSelectedRow() const;

    // Toggle the playback button state; also decides whether starting is allowed based on whether the list is empty
    void setRunning(bool running);

    // Toggle the recording button state; while recording, also lock editing, import/export and playback
    void setRecording(bool recording);

    // Delivery after recording ends (stopped or limit reached): hint on empty list, three-way dialog otherwise
    void onRecordFinished(const TaskResult &res);
    void deliverRecording(const std::vector<Action> &actions);

    // ---- Data ----
    std::vector<Action> m_actions;
    bool m_recording = false;
    ThreadManager &m_threadMgr;

    // ---- Widgets ----
    wxButton *m_btnStartRecord{};
    wxButton *m_btnStopRecord{};

    wxListCtrl *m_listCtrl{};
    wxButton *m_btnAdd{};
    wxButton *m_btnEdit{};
    wxButton *m_btnDelete{};
    wxButton *m_btnMoveUp{};
    wxButton *m_btnMoveDown{};
    wxButton *m_btnClear{};

    wxChoice *m_choiceRunMode{};
    wxChoice *m_choiceSpeed{};
    wxSpinCtrl *m_spinDuration{};
    wxSpinCtrl *m_spinCount{};
    wxSpinCtrl *m_spinStartDelay{};

    wxButton *m_btnStartPlayback{};
    wxButton *m_btnStopPlayback{};
    wxButton *m_btnImportScript{};
    wxButton *m_btnExportScript{};
};
