#pragma once

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>

// ============================================================
// 设置面板。
//
// 数据来源：
//   - 通用 / 计时器 / 置顶 / 配置路径：ConfigManager 单例
//   - 热键：HotkeyManager（注入引用），数据本身存在 ConfigManager
//
// 构造时 refreshFromConfig() 把当前配置同步到各控件；
// 用户操作走 ConfigManager::setXxx / HotkeyManager::setHotkey，内部落盘。
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

    // 从 ConfigManager 读当前配置，同步到所有控件（不触发事件回写）
    void refreshFromConfig();

    // 重填热键列表（数据源：ConfigManager::config().hotkeys）
    void refreshHotkeyList();

    // 刷新配置文件路径显示
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

    // ---- 服务（外部注入）----
    HotkeyManager &m_hotkeyMgr;

    // ---- 通用 ----
    wxChoice *m_choiceLanguage{};
    wxChoice *m_choiceTimerMode{};
    // 混合模式的末尾忙等时长；仅 Hybrid 时启用
    wxSpinCtrl *m_spinHybridMs{};
    wxCheckBox *m_chkAlwaysOnTop{};

    // ---- 录制 ----
    wxSpinCtrl *m_spinRecMoveInterval{};
    wxSpinCtrl *m_spinRecStartDelay{};
    wxSpinCtrl *m_spinRecMaxActions{};
    wxCheckBox *m_chkRecIgnoreInjected{};

    // ---- 热键 ----
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
