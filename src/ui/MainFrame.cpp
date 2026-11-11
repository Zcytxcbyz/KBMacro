#include "ui/MainFrame.hpp"

#include <wx/display.h>
#include <wx/notebook.h>

#include <algorithm>
#include <windows.h>

#include "common/Log.hpp"
#include "common/Timer.hpp"
#include "managers/ConfigManager.hpp"
#include "managers/HotkeyManager.hpp"
#include "managers/LanguageManager.hpp"
#include "managers/ThreadManager.hpp"

#include "win32/Input.hpp"

#include "ui/KBMacroStatusBar.hpp"
#include "ui/panels/ClickerPanel.hpp"
#include "ui/panels/KeyboardPanel.hpp"
#include "ui/panels/RecorderPanel.hpp"
#include "ui/panels/SettingsPanel.hpp"
#include "ui/panels/AboutPanel.hpp"

namespace {

    // 从 Config 读取初始窗口位置。
    //   - x/y < 0  → 未设置，走 wxDefaultPosition
    //   - 不在虚拟桌面范围内 → 走 wxDefaultPosition（比如副屏被拔掉了）
    wxPoint initialPosition() {
        const auto &w = ConfigManager::instance().config().window;
        if (w.x < 0 || w.y < 0) return wxDefaultPosition;

        const int vx = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int vy = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int vw = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int vh = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

        // 只要求窗口至少有一部分在虚拟桌面内
        const bool visible = (w.x + w.width > vx) && (w.x < vx + vw) &&
                             (w.y + w.height > vy) && (w.y < vy + vh);

        return visible ? wxPoint(w.x, w.y) : wxDefaultPosition;
    }

    // 从 Config 读取初始窗口样式（置顶与否）
    long initialStyle() {
        return ConfigManager::instance().config().alwaysOnTop
                   ? wxDEFAULT_FRAME_STYLE | wxSTAY_ON_TOP
                   : wxDEFAULT_FRAME_STYLE;
    }

    // 按配置创建计时器实例。
    // 切换模式时整体替换，不原地改参数——ITimer 是 const 的、且可能正被
    // 运行中的线程持有，替换比改状态安全。
    std::shared_ptr<ITimer> createTimer() {
        const auto &cfg = ConfigManager::instance().config();
        switch (cfg.timerMode) {
            case TimerMode::Spin:
                return std::make_shared<SpinTimer>();
            case TimerMode::Hybrid:
                return std::make_shared<HybridTimer>(ITimer::Duration{cfg.timerHybridSpinMs});
            case TimerMode::Sleep:
            default:
                return std::make_shared<SleepTimer>();
        }
    }

    // 初始几何：用配置里的尺寸，夹到"窗口将要放的那个显示器"可用区（已扣任务栏）的 90%。
    // 内容装不下由各页的滚动条兜底，不在这里按内容撑开。
    void applyInitialGeometry(wxFrame *frame) {
        const auto &w = ConfigManager::instance().config().window;

        // 用窗口将要放置的显示器；配置里没有位置时用主显示器
        const wxPoint pos = initialPosition();
        unsigned displayIdx = 0;
        if (pos != wxDefaultPosition) {
            if (const int idx = wxDisplay::GetFromPoint(pos); idx != wxNOT_FOUND) {
                displayIdx = static_cast<unsigned>(idx);
            }
        }

        const wxRect area = wxDisplay(displayIdx).GetClientArea();
        const int maxW    = area.width * 9 / 10;
        const int maxH    = area.height * 9 / 10;

        // 初始尺寸只看配置：五页都是滚动窗口，装不下就滚，不需要按内容撑开
        const wxSize target(std::min(w.width, maxW), std::min(w.height, maxH));

        frame->SetSize(target);

        if (pos == wxDefaultPosition) {
            frame->Centre();
        } else {
            frame->Move(pos);
        }

        LOG_DEBUG("mainframe: geometry config {}x{}, display {}x{}, window {}x{}",
                  w.width, w.height, area.width, area.height, target.x, target.y);
    }

} // namespace

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, TR(IDS_APP_TITLE),
              wxDefaultPosition, wxDefaultSize, initialStyle()) {
    LOG_INFO("mainframe: creating");

    // 窗口与任务栏图标（资源见 res/resource.rc 的 "KBMACRO_ICON"）
    SetIcon(wxIcon("KBMACRO_ICON", wxBITMAP_TYPE_ICO_RESOURCE));

    // 输入注入层初始化：只记一条"虚拟屏 + DPI"（干跑模式下会多一条 WARN）
    input::init();

    // 计时器：按配置创建，切换模式时由 rebuildTimer() 整体替换
    m_timer = createTimer();
    LOG_DEBUG("mainframe: timer ready: mode={}, spinTail={}ms",
              timerModeToString(ConfigManager::instance().config().timerMode),
              ConfigManager::instance().config().timerHybridSpinMs);
    // ---- 1. 先创建 HotkeyManager（只依赖 HWND）----
    m_hotkeyMgr = std::make_unique<HotkeyManager>(
        GetHWND(),
        [this](const HotkeyAction action) {
            dispatchHotkeyAction(action);
        });
    Bind(wxEVT_HOTKEY, &MainFrame::onHotkey, this);

    // ---- 1.5 创建 ThreadManager：面板构造时要注入它的引用，必须早于面板 ----
    // 它内部持 m_timer 的引用，所以必须排在 createTimer() 之后
    m_threadMgr = std::make_unique<ThreadManager>(m_timer, *this);

    // ---- 2. 组装 UI ----
    m_notebook = new wxNotebook(this, wxID_ANY);

    // 标签页1：鼠标连点器
    m_clickerPanel = new ClickerPanel(m_notebook, *m_threadMgr);
    m_notebook->AddPage(m_clickerPanel, TR(IDS_TAB_CLICKER));

    // 标签页2：键盘模拟器
    m_keyboardPanel = new KeyboardPanel(m_notebook, *m_threadMgr, m_timer);
    m_notebook->AddPage(m_keyboardPanel, TR(IDS_TAB_KEYBOARD));

    // 标签页3：录制与编排
    m_recorderPanel = new RecorderPanel(m_notebook, *m_threadMgr);
    m_notebook->AddPage(m_recorderPanel, TR(IDS_TAB_RECORDER));

    // 标签页4：设置（注入 HotkeyManager）
    m_settingsPanel = new SettingsPanel(m_notebook, *m_hotkeyMgr);
    m_notebook->AddPage(m_settingsPanel, TR(IDS_TAB_SETTINGS));

    // 标签页5：关于
    m_aboutPanel = new AboutPanel(m_notebook);
    m_notebook->AddPage(m_aboutPanel, TR(IDS_TAB_ABOUT));

    // notebook 必须由 sizer 管，否则它不会被 frame 拉伸：
    // 窗口拖大它不变、窗口缩小就直接裁剪（旧实现就是这个毛病）
    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(m_notebook, 1, wxEXPAND);
    SetSizer(root);

    // ---- 3. 恢复上次选中的标签页 ----
    const int savedTab = ConfigManager::instance().config().tabIndex;
    if (savedTab >= 0 && savedTab < m_notebook->GetPageCount()) {
        m_notebook->SetSelection(savedTab);
    }

    wxWindowBase::SetBackgroundColour(wxColour(240, 240, 240));

    // ---- 4. 状态栏 ----
    m_statusBar = new KBMacroStatusBar(this);
    wxFrameBase::SetStatusBar(m_statusBar);
    m_statusBar->SetMessage(TR(IDS_STATUS_READY));

    // ---- 5. 按内容与屏幕确定初始几何（必须在面板建好、SetSizer 之后）----
    applyInitialGeometry(this);

    // ---- 6. 如果配置里记着"最大化"，最大化 ----
    if (ConfigManager::instance().config().window.maximized) {
        wxTopLevelWindowMSW::Maximize(true);
    }

    // ---- 7. 最后注册热键（面板都构造好了，之后触发的回调才能安全分发）----
    m_hotkeyMgr->registerAll();
    reportHotkeyFailures();

    // ---- 8. 状态栏三态由计时器驱动：任务状态只在 ThreadManager 里，
    //        面板各自刷自己的按钮，主窗口靠轮询汇总（迟 ≤300ms，无耦合）
    Bind(wxEVT_TIMER, &MainFrame::onStatusTimer, this);
    m_statusTimer.Start(300);

    // ---- 9. 关闭时保存几何 + 标签页 ----
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::onClose, this);
}

MainFrame::~MainFrame() {
    // 先停掉所有任务（阻塞 join），再清掉还在队列里的通知事件。
    // 不清的话：worker 退出前 CallAfter 的事件会一直挂到 ~wxEvtHandler 才被删，
    // 那时面板早已析构，lambda 里捕获的面板指针就是悬垂的。
    m_threadMgr.reset();
    DeletePendingEvents();
}

void MainFrame::rebuildTimer() {
    m_timer = createTimer();

    const auto &cfg = ConfigManager::instance().config();
    LOG_DEBUG("mainframe: timer rebuilt: mode={}, spinTail={}ms",
              timerModeToString(cfg.timerMode), cfg.timerHybridSpinMs);
}

// ============================================================
// wxEVT_HOTKEY 处理：转发给 HotkeyManager
// ============================================================
// ReSharper disable once CppParameterMayBeConstPtrOrRef
// ReSharper disable once CppMemberFunctionMayBeConst
void MainFrame::onHotkey(wxKeyEvent &event) {
    // ReSharper disable once CppDeclaratorNeverUsed
    const int id = event.GetId();
    LOG_DEBUG("mainframe: wxEVT_HOTKEY id={} (index={})",
              id, id - HotkeyManager::kHotkeyIdBase);
    m_hotkeyMgr->dispatch(event.GetId());
}

// ============================================================
// 保存窗口几何 + 当前标签页
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void MainFrame::saveWindowState() {
    auto &cfgMgr = ConfigManager::instance();

    // ---- 窗口几何 ----
    // 在现存的 Config::Window 上打补丁，而不是新建一个——这样"最大化时
    // 不改 x/y/width/height"的语义自然成立（保留上次正常状态的值）。
    Config::Window w = cfgMgr.config().window;

    if (IsMaximized()) {
        // 最大化：只更新标志位，几何保留上次"正常状态"的值
        w.maximized = true;
    } else if (IsIconized()) {
        // 最小化：几何信息不可靠，什么都不改（保留上次的值）
        // 标志位也不动，保持上次的选择
    } else {
        // 正常状态：更新几何 + 清掉最大化标志
        w.maximized = false;
        const wxPoint pos = GetPosition();
        const wxSize size = GetSize();
        w.x = pos.x;
        w.y = pos.y;
        w.width = size.x;
        w.height = size.y;
    }
    cfgMgr.setWindow(w);

    // ---- 标签页 ----
    if (m_notebook) {
        cfgMgr.setTabIndex(m_notebook->GetSelection());
    }
}

// ============================================================
// wxEVT_CLOSE_WINDOW：保存窗口几何 + 当前标签页
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void MainFrame::onClose(wxCloseEvent &event) {
    // 记在 saveWindowState 之前：关窗真正可能卡住的是之后的
    // ~MainFrame → ThreadManager::StopAll（4 次阻塞 join），不是存配置
    LOG_INFO("mainframe: closing");
    saveWindowState();

    // 放行关闭流程
    event.Skip();
}

// ============================================================
// 热键分发：按动作调用对应面板的公开方法
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void MainFrame::dispatchHotkeyAction(const HotkeyAction action) {
    LOG_DEBUG("mainframe: dispatch action={}", static_cast<int>(action));
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

        case HotkeyAction::SendOnce:
            m_keyboardPanel->sendOnce();
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
    if (failed <= 0) {
        LOG_INFO("mainframe: all configured hotkeys registered");
        return;
    }

    const wxString msg = wxString::Format(TR(IDS_STATUS_HOTKEY_FAILED).data(), failed);
    m_statusBar->SetNotice(msg);
    LOG_WARN("mainframe: {} hotkey(s) failed to register", failed);
}

// ============================================================
// 状态栏：三态 = 就绪 / 运行中（列出在跑的任务）
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void MainFrame::refreshStatus() {
    static constexpr struct {
        TaskKind kind;
        int resId;
    } kTasks[] = {
        {TaskKind::Clicker,  IDS_TASK_CLICKER},
        {TaskKind::Keyboard, IDS_TASK_KEYBOARD},
        {TaskKind::Record,   IDS_TASK_RECORD},
        {TaskKind::Playback, IDS_TASK_PLAYBACK},
    };

    wxString names;
    for (const auto &task: kTasks) {
        if (!m_threadMgr || !m_threadMgr->IsRunning(task.kind)) {
            continue;
        }

        if (!names.empty()) {
            names += TR(IDS_STATUS_TASK_SEP);
        }
        names += TR(task.resId);
    }

    if (names.empty()) {
        m_statusBar->SetMessage(TR(IDS_STATUS_READY));
        return;
    }

    m_statusBar->SetMessage(wxString::Format(TR(IDS_STATUS_RUNNING).data(), names.wc_str()));
}

void MainFrame::onStatusTimer(wxTimerEvent &) {
    refreshStatus();
}
