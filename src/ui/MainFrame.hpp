#pragma once

#include <memory>

#include <wx/wx.h>

#include "common/HotkeyAction.hpp"

// 前向声明（避免在头文件里引入一堆面板实现）
class wxNotebook;
class HotkeyManager;
class KBMacroStatusBar;
class ClickerPanel;
class KeyboardPanel;
class RecorderPanel;
class SettingsPanel;
class AboutPanel;

// 主窗口。
//
// 职责：
//   - 创建并持有 wxNotebook，组装五个标签页
//   - 持有 HotkeyManager，接收 WM_HOTKEY 并分发到对应面板
//   - 处理窗口级样式、尺寸、位置、标签页记忆
//
// 热键分发路由：
//   HotkeyManager::TriggerCallback -> MainFrame::dispatchHotkeyAction
//     -> 各面板的 startWithCurrentParams() / stop() / ...
//   MainFrame 不直接操作 ThreadManager；面板自己负责调（等模块落地）。
class MainFrame : public wxFrame {
public:
    MainFrame();
    ~MainFrame() override;

private:
    // HotkeyManager 的回调入口：switch 到对应面板方法
    void dispatchHotkeyAction(HotkeyAction action);

    // wxEVT_HOTKEY 处理：转发给 HotkeyManager::dispatch
    void onHotkey(wxKeyEvent &event);

    // wxEVT_CLOSE_WINDOW 处理：保存窗口几何 + 当前标签页，然后放行关闭
    void onClose(wxCloseEvent &event);

    // 注册失败时把数量写到状态栏
    void reportHotkeyFailures();

    // ---- 服务 ----
    std::unique_ptr<HotkeyManager> m_hotkeyMgr;

    // ---- 布局 ----
    wxNotebook *m_notebook{};

    // ---- 面板（热键分发的目标）----
    ClickerPanel *m_clickerPanel{};
    KeyboardPanel *m_keyboardPanel{};
    RecorderPanel *m_recorderPanel{};
    SettingsPanel *m_settingsPanel{};
    AboutPanel *m_aboutPanel{};

    // ---- 状态栏 ----
    KBMacroStatusBar *m_statusBar{};
};
