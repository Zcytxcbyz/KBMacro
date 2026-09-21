#pragma once

#include <wx/wx.h>

#include "common/KeyboardParams.hpp"
#include "common/Types.hpp"

// 键盘模拟器面板。
//
// 与 ThreadManager 的对接在模块落地后补。本阶段：
//   - 提供公开方法（供热键路径调用）
//   - 支持参数的导入 / 导出（*.keyboard.json）
class KeyboardPanel : public wxPanel {
public:
    explicit KeyboardPanel(wxWindow *parent);

    // 供 MainFrame 从热键路径调用，与用户点击"开始"/"停止"按钮等价。
    void startWithCurrentParams();
    void stop();

private:
    void buildUi();

    // 从控件组装 KeyboardParams
    [[nodiscard]] KeyboardParams collectParams() const;

    // 把 KeyboardParams 同步到控件
    void applyParams(const KeyboardParams &p);

    void onSetKey(wxCommandEvent &event);

    void onSendOnce(wxCommandEvent &event);

    void onStart(wxCommandEvent &event);

    void onStop(wxCommandEvent &event);

    void onImport(wxCommandEvent &event);

    void onExport(wxCommandEvent &event);

    void refreshEnableStates();
    void onRunModeChanged(wxCommandEvent &event);

    // 刷新按键显示框
    void refreshKeyDisplay();

    // ---- 按键 ----
    Hotkey m_hotkey;
    wxTextCtrl *m_keyDisplay{};
    wxButton *m_btnSetKey{};

    // ---- 参数 ----
    wxSpinCtrl *m_spinInterval{};
    wxSpinCtrl *m_spinHoldTime{};

    // ---- 运行模式 ----
    wxChoice *m_choiceRunMode{};
    wxSpinCtrl *m_spinDuration{};
    wxSpinCtrl *m_spinCount{};

    // ---- 操作 ----
    wxButton *m_btnSendOnce{};
    wxButton *m_btnStart{};
    wxButton *m_btnStop{};
    wxButton *m_btnImport{};
    wxButton *m_btnExport{};
};
