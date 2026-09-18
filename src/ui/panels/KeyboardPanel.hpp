#pragma once

#include <wx/wx.h>

// 键盘模拟器面板。
//
// 当前为纯 UI 阶段：控件已布局，按钮事件只记录日志，不接实际逻辑。
// 热键捕获对话框、控件启用/禁用、ThreadManager 对接等后续补齐。
class KeyboardPanel : public wxPanel {
public:
    explicit KeyboardPanel(wxWindow *parent);

private:
    void buildUi();

    void onSetKey(wxCommandEvent &event);

    void onSendOnce(wxCommandEvent &event);

    void onStart(wxCommandEvent &event);

    void onStop(wxCommandEvent &event);

    void onImport(wxCommandEvent &event);

    void onExport(wxCommandEvent &event);

    // ---- 按键 ----
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
