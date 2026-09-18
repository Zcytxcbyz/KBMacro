#pragma once

#include <array>
#include <wx/wx.h>
#include <wx/listctrl.h>

#include "common/Types.hpp"

// 设置面板。
//
// 当前为纯 UI 阶段：控件已布局，弹窗交互已实现，破坏性操作只记录日志。
// ConfigManager 与 HotkeyManager 对接后续补齐。
class SettingsPanel : public wxPanel {
public:
    explicit SettingsPanel(wxWindow *parent);

private:
    void buildUi();

    void buildGeneralGroup(wxSizer *parent);
    void buildHotkeyGroup(wxSizer *parent);
    void buildConfigGroup(wxSizer *parent);
    void buildResetGroup(wxSizer *parent);

    void onLanguageChanged(wxCommandEvent &event);
    void onTimerModeChanged(const wxCommandEvent &event);
    void onAlwaysOnTopToggled(wxCommandEvent &event);
    void onModifyHotkey(wxCommandEvent &event);
    void onExportConfig(wxCommandEvent &event);
    void onClearAdaptation(wxCommandEvent &event);
    void onClearSpinConfirm(wxCommandEvent &event);
    void onResetDefaults(wxCommandEvent &event);
    void onDeletePortableConfig(wxCommandEvent &event);
    void onHotkeySelectionChanged(wxListEvent &event);

    // ---- 通用 ----
    wxChoice *m_choiceLanguage{};
    wxChoice *m_choiceTimerMode{};
    wxCheckBox *m_chkAlwaysOnTop{};

    // ---- 热键 ----
    static constexpr int kHotkeyCount = 10;
    std::array<Hotkey, kHotkeyCount> m_hotkeys{};
    wxListCtrl *m_hotkeyList{};
    wxButton *m_btnModifyHotkey{};

    // ---- 配置管理 ----
    wxStaticText *m_configPathText{};
    wxButton *m_btnExportConfig{};

    // ---- 重置 ----
    wxButton *m_btnClearAdaptation{};
    wxButton *m_btnClearSpinConfirm{};
    wxButton *m_btnResetDefaults{};
    wxButton *m_btnDeletePortableConfig{};
};
