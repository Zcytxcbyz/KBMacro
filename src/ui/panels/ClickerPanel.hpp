#pragma once

#include <wx/wx.h>

#include "common/ClickerParams.hpp"

// 鼠标连点器面板。
//
// 与 ThreadManager 的对接在模块落地后补。本阶段：
//   - 提供公开方法（供热键路径调用）
//   - 支持参数的导入 / 导出（*.clicker.json）
class ClickerPanel : public wxPanel {
public:
    explicit ClickerPanel(wxWindow *parent);

    // 供 MainFrame 从热键路径调用，与用户点击"开始"/"停止"按钮等价。
    // 从当前控件值组装 ClickerParams，交给 ThreadManager 启动。
    // 具体实现等 Clicker 模块落地，当前只打日志占位。
    void startWithCurrentParams();
    void stop();

private:
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
