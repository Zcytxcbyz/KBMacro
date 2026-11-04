#pragma once

#include <memory>

#include <wx/wx.h>

#include "common/ITimer.hpp"
#include "common/KeyboardParams.hpp"
#include "common/Types.hpp"

class ThreadManager;

// 键盘模拟器面板。
//
//   - 参数在本面板组装（collectParams / applyParams），启动时快照给 ThreadManager
//   - 计时器从 MainFrame 借用（引用 shared_ptr 本身，切换模式后自动跟着换新的）
//   - "发送一次"是同步的一次性动作，不走线程也不经过 ThreadManager
class KeyboardPanel : public wxPanel {
public:
    KeyboardPanel(wxWindow *parent, ThreadManager &threadMgr,
                  std::shared_ptr<ITimer> &timerRef);

    // 供 MainFrame 从热键路径调用，与用户点击"开始"/"停止"按钮等价。
    void startWithCurrentParams();
    void stop();

    // 供 MainFrame 从热键路径调用，与用户点击"发送一次"按钮等价。
    // 不涉及 RunSpec / 线程，直接调 SendInput 发一次当前热键。
    void sendOnce();

private:
    // 切换"运行中"的按钮状态：运行中禁用开始、启用停止
    void setRunning(bool running);

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

    ThreadManager &m_threadMgr;
    std::shared_ptr<ITimer> &m_timerRef;

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
