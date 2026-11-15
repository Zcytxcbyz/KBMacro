#pragma once

#include <wx/listctrl.h>
#include <wx/scrolwin.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <wx/spinctrl.h>
#include <wx/wx.h>

// ============================================================
// Settings panel.
//
// Data sources:
//   - General / timer / always-on-top / config path: ConfigManager singleton
//   - Hotkeys: HotkeyManager (injected reference); the data itself lives in ConfigManager
//
// Construction calls refreshFromConfig() to sync the current config into all widgets;
// user actions go through ConfigManager::setXxx / HotkeyManager::setHotkey, which persist internally.
// ============================================================

class HotkeyManager;

class SettingsPanel : public wxScrolledWindow {
public:
    SettingsPanel(wxWindow *parent, HotkeyManager &hotkeyMgr);

private:
    void buildUi();

    void buildGeneralGroup(wxSizer *parent);
    void buildHotkeyGroup(wxSizer *parent);
    void buildRecorderGroup(wxSizer *parent);
    void buildConfigGroup(wxSizer *parent);
    void buildResetGroup(wxSizer *parent);

    // Read the current config from ConfigManager and sync it to all widgets (no event write-back)
    void refreshFromConfig();

    // Refill the hotkey list (data source: ConfigManager::config().hotkeys)
    void refreshHotkeyList();

    // Refresh the config file path display
    void refreshConfigPathText();

    void onLanguageChanged(wxCommandEvent &event);
    void onTimerModeChanged(const wxCommandEvent &event);
    void onHybridSpinChanged(wxCommandEvent &event);
    void onRecorderMoveIntervalChanged(wxCommandEvent &event);
    void onRecorderIgnoreInjectedToggled(wxCommandEvent &event);
    void onRecorderStartDelayChanged(wxCommandEvent &event);
    void onRecorderMaxActionsChanged(wxCommandEvent &event);
    void onAlwaysOnTopToggled(wxCommandEvent &event);
    void onModifyHotkey(wxCommandEvent &event);
    void onExportConfig(wxCommandEvent &event);
    void onClearAdaptation(wxCommandEvent &event);
    void onClearSpinConfirm(wxCommandEvent &event);
    void onResetDefaults(wxCommandEvent &event);
    void onDeletePortableConfig(wxCommandEvent &event);
    void onHotkeySelectionChanged(wxListEvent &event);
    void onHotkeyChecked(wxListEvent &event);

    // ---- Services (injected) ----
    HotkeyManager &m_hotkeyMgr;

    // ---- General ----
    wxChoice *m_choiceLanguage{};
    wxChoice *m_choiceTimerMode{};
    // End-of-hybrid busy-wait duration; enabled only in Hybrid mode
    wxSpinCtrl *m_spinHybridMs{};
    wxCheckBox *m_chkAlwaysOnTop{};

    // ---- Recording ----
    wxSpinCtrl *m_spinRecMoveInterval{};
    wxSpinCtrl *m_spinRecStartDelay{};
    wxSpinCtrl *m_spinRecMaxActions{};
    wxCheckBox *m_chkRecIgnoreInjected{};

    // ---- Hotkeys ----
    wxListCtrl *m_hotkeyList{};
    wxButton *m_btnModifyHotkey{};

    // ---- Config management ----
    wxStaticText *m_configPathText{};
    wxButton *m_btnExportConfig{};

    // ---- Reset ----
    wxButton *m_btnClearAdaptation{};
    wxButton *m_btnClearSpinConfirm{};
    wxButton *m_btnResetDefaults{};
    wxButton *m_btnDeletePortableConfig{};
};
