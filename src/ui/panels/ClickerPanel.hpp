#pragma once

#include <wx/wx.h>

// 鼠标连点器面板。
//
// 当前为纯 UI 阶段：控件已布局，按钮事件只记录日志，不接实际逻辑。
// 控件启用/禁用、数值校验、ThreadManager 对接等后续补齐。
class ClickerPanel : public wxPanel {
public:
    explicit ClickerPanel(wxWindow *parent);

private:
    void buildUi();

    void onCapturePos(wxCommandEvent &event);

    void onIgnorePos(wxCommandEvent &event);

    void onStart(wxCommandEvent &event);

    void onStop(wxCommandEvent &event);

    void onImport(wxCommandEvent &event);

    void onExport(wxCommandEvent &event);

    // ---- 坐标 ----
    wxTextCtrl *m_x{};
    wxTextCtrl *m_y{};
    wxButton *m_btnCapture{};
    wxCheckBox *m_chkIgnorePos{};

    // ---- 点击设置 ----
    wxChoice *m_choiceType{};
    wxSpinCtrl *m_spinWheelSteps{};
    wxSpinCtrl *m_spinInterval{};
    wxSpinCtrl *m_spinHoldTime{};

    // ---- 运行模式 ----
    wxChoice *m_choiceRunMode{};
    wxSpinCtrl *m_spinDuration{};
    wxSpinCtrl *m_spinCount{};

    // ---- 操作 ----
    wxButton *m_btnStart{};
    wxButton *m_btnStop{};
    wxButton *m_btnImport{};
    wxButton *m_btnExport{};
};
