#pragma once

#include <wx/wx.h>

#include "common/Types.hpp"

// 键盘模拟器面板。
//
// 当前为纯 UI 阶段：控件已布局，按钮事件只记录日志，不接实际逻辑。
// 热键捕获对话框、控件启用/禁用、ThreadManager 对接等后续补齐。
class KeyboardPanel : public wxPanel {
public:
    explicit KeyboardPanel(wxWindow *parent);

    // 供 MainFrame 从热键路径调用，与用户点击"开始"/"停止"按钮等价。
    void startWithCurrentParams();
    void stop();

private:
    void buildUi();

    void onSetKey(wxCommandEvent &event);

    void onSendOnce(wxCommandEvent &event);

    void onStart(wxCommandEvent &event);

    void onStop(wxCommandEvent &event);

    void onImport(wxCommandEvent &event);

    void onExport(wxCommandEvent &event);

    void refreshEnableStates();
    void onRunModeChanged(wxCommandEvent &event);

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
