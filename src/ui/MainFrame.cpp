#include "ui/MainFrame.hpp"

#include <wx/notebook.h>

#include "common/Log.hpp"
#include "managers/ConfigManager.hpp"
#include "managers/HotkeyManager.hpp"
#include "managers/LanguageManager.hpp"

#include "ui/KBMacroStatusBar.hpp"
#include "ui/panels/ClickerPanel.hpp"
#include "ui/panels/KeyboardPanel.hpp"
#include "ui/panels/RecorderPanel.hpp"
#include "ui/panels/SettingsPanel.hpp"
#include "ui/panels/AboutPanel.hpp"

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, TR(IDS_APP_TITLE),
              wxDefaultPosition, wxSize(550, 710),
              ConfigManager::instance().config().alwaysOnTop
                  ? wxDEFAULT_FRAME_STYLE | wxSTAY_ON_TOP
                  : wxDEFAULT_FRAME_STYLE) {
    // ---- 1. 先创建 HotkeyManager（只依赖 HWND）----
    // 触发回调在 dispatch 时才执行，此时面板都已构造完毕。
    m_hotkeyMgr = std::make_unique<HotkeyManager>(
        GetHWND(),
        [this](const HotkeyAction action) {
            dispatchHotkeyAction(action);
        });
    Bind(wxEVT_HOTKEY, &MainFrame::onHotkey, this);

    // ---- 2. 组装 UI ----
    auto *notebook = new wxNotebook(this, wxID_ANY);

    // 标签页1：鼠标连点器
    m_clickerPanel = new ClickerPanel(notebook);
    notebook->AddPage(m_clickerPanel, TR(IDS_TAB_CLICKER));

    // 标签页2：键盘模拟器
    m_keyboardPanel = new KeyboardPanel(notebook);
    notebook->AddPage(m_keyboardPanel, TR(IDS_TAB_KEYBOARD));

    // 标签页3：录制与编排
    m_recorderPanel = new RecorderPanel(notebook);
    notebook->AddPage(m_recorderPanel, TR(IDS_TAB_RECORDER));

    // 标签页4：设置（注入 HotkeyManager）
    m_settingsPanel = new SettingsPanel(notebook, *m_hotkeyMgr);
    notebook->AddPage(m_settingsPanel, TR(IDS_TAB_SETTINGS));

    // 标签页5：关于
    m_aboutPanel = new AboutPanel(notebook);
    notebook->AddPage(m_aboutPanel, TR(IDS_TAB_ABOUT));

    wxWindowBase::SetBackgroundColour(wxColour(240, 240, 240));

    // ---- 3. 状态栏 ----
    m_statusBar = new KBMacroStatusBar(this);
    wxFrameBase::SetStatusBar(m_statusBar);
    m_statusBar->SetMessage(TR(IDS_STATUS_READY));

    // ---- 4. 最后注册热键（面板都构造好了，之后触发的回调才能安全分发）----
    m_hotkeyMgr->registerAll();
    reportHotkeyFailures();
}

MainFrame::~MainFrame() = default;

// ============================================================
// wxEVT_HOTKEY 处理：转发给 HotkeyManager
// ============================================================
// ReSharper disable once CppParameterMayBeConstPtrOrRef
// ReSharper disable once CppMemberFunctionMayBeConst
void MainFrame::onHotkey(wxKeyEvent &event) {
    m_hotkeyMgr->dispatch(event.GetId());
}

// ============================================================
// 热键分发：按动作调用对应面板的公开方法
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void MainFrame::dispatchHotkeyAction(const HotkeyAction action) {
    switch (action) {
        case HotkeyAction::StartClicker:
            m_clickerPanel->startWithCurrentParams();
            break;
        case HotkeyAction::StopClicker:
            m_clickerPanel->stop();
            break;

        case HotkeyAction::StartKeyboard:
            m_keyboardPanel->startWithCurrentParams();
            break;
        case HotkeyAction::StopKeyboard:
            m_keyboardPanel->stop();
            break;

        case HotkeyAction::StartRecord:
            m_recorderPanel->startRecording();
            break;
        case HotkeyAction::StopRecord:
            m_recorderPanel->stopRecording();
            break;

        case HotkeyAction::StartPlayback:
            m_recorderPanel->startPlayback();
            break;
        case HotkeyAction::StopPlayback:
            m_recorderPanel->stopPlayback();
            break;

        // "全部开始"：连点器和键盘独立，可以同时跑；
        // 录制与回放互斥，不在这里自动启动——用户手动选一个。
        case HotkeyAction::StartAll:
            m_clickerPanel->startWithCurrentParams();
            m_keyboardPanel->startWithCurrentParams();
            break;

        case HotkeyAction::StopAll:
            m_clickerPanel->stop();
            m_keyboardPanel->stop();
            m_recorderPanel->stopRecording();
            m_recorderPanel->stopPlayback();
            break;
    }
}

// ============================================================
// 注册失败汇总到状态栏
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void MainFrame::reportHotkeyFailures() {
    const int failed = m_hotkeyMgr->failedCount();
    if (failed <= 0) return;

    const wxString msg = wxString::Format(TR(IDS_STATUS_HOTKEY_FAILED).data(), failed);
    m_statusBar->SetMessage(msg);
    LOG_WARN("mainframe: {} hotkey(s) failed to register", failed);
}
