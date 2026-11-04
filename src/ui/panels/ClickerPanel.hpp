#pragma once

#include <wx/wx.h>

#include "common/ClickerParams.hpp"

class ThreadManager;

// 鼠标连点器面板。
//
//   - 参数在本面板组装（collectParams / applyParams），启动时快照给 ThreadManager
//   - 是否真在跑由 ThreadManager 的 Slot 状态决定，面板只按结果切按钮
class ClickerPanel : public wxPanel {
public:
    ClickerPanel(wxWindow *parent, ThreadManager &threadMgr);

    // 供 MainFrame 从热键路径调用，与用户点击"开始"/"停止"按钮等价。
    // 从当前控件值组装 ClickerParams，交给 ThreadManager 启动。
    // 具体实现等 Clicker 模块落地，当前只打日志占位。
    void startWithCurrentParams();
    void stop();

private:
    // 切换"运行中"的按钮状态：运行中禁用开始、启用停止
    void setRunning(bool running);

    void buildUi();

    // 从控件组装 ClickerParams
    [[nodiscard]] ClickerParams collectParams() const;

    // 把 ClickerParams 同步到控件；末尾会调 refreshEnableStates
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
