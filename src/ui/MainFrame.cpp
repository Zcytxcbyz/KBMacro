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

    // read the initial window position from Config.
    //   - x/y < 0  -> not set, use wxDefaultPosition
    //   - outside the virtual desktop -> use wxDefaultPosition (e.g. a secondary monitor was unplugged)
    wxPoint initialPosition() {
        const auto &w = ConfigManager::instance().config().window;
        if (w.x < 0 || w.y < 0) return wxDefaultPosition;

        const int vx = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int vy = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int vw = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int vh = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

        // only require the window to be at least partly on the virtual desktop
        const bool visible = (w.x + w.width > vx) && (w.x < vx + vw) &&
                             (w.y + w.height > vy) && (w.y < vy + vh);

        return visible ? wxPoint(w.x, w.y) : wxDefaultPosition;
    }

    // read the initial window style from Config (always-on-top or not)
    long initialStyle() {
        return ConfigManager::instance().config().alwaysOnTop
                   ? wxDEFAULT_FRAME_STYLE | wxSTAY_ON_TOP
                   : wxDEFAULT_FRAME_STYLE;
    }

    // create the timer instance from the config.
    // replaced wholesale on mode change, never tweaked in place: ITimer is const and may be
    // held by a running thread, so replacing is safer than mutating state.
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

    // initial geometry: use the configured size, clamped to 90% of the available area (taskbar excluded)
    // of the display the window will land on. Content overflow falls back to each page's scrollbar.
    void applyInitialGeometry(wxFrame *frame) {
        const auto &w = ConfigManager::instance().config().window;

        // use the display the window will be placed on; the primary one when no position is configured
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

        // initial size comes only from config: all five pages scroll, no need to grow to content
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

    // icon: numeric resource id + MAKEINTRESOURCEW.
    // do not use wxICON(IDI_APP_ICON): wxICON "stringizes" the argument into "IDI_APP_ICON",
    // while resource.h has already expanded IDI_APP_ICON to the number 101 in resource.rc,
    // so a lookup by name fails.
    if (const auto appIcon = static_cast<HICON>(::LoadImageW(
            ::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0,
            LR_DEFAULTSIZE | LR_DEFAULTCOLOR))) {
        wxIcon icon;
        icon.CreateFromHICON(appIcon);   // CreateFromHICON copies the icon internally, no double free
        SetIcon(icon);
        LOG_DEBUG("mainframe: icon applied from resource id {}", IDI_APP_ICON);
            } else {
                LOG_WARN("mainframe: icon resource id {} failed, GetLastError={}", IDI_APP_ICON,
                         ::GetLastError());
            }

    // init the input injection layer: logs one "virtual screen + DPI" line (dry-run mode adds a WARN)
    input::init();

    // timer: created from config, replaced wholesale by rebuildTimer() on mode change
    m_timer = createTimer();
    LOG_DEBUG("mainframe: timer ready: mode={}, spinTail={}ms",
              timerModeToString(ConfigManager::instance().config().timerMode),
              ConfigManager::instance().config().timerHybridSpinMs);
    // ---- 1. create HotkeyManager first (depends only on HWND) ----
    m_hotkeyMgr = std::make_unique<HotkeyManager>(
        GetHWND(),
        [this](const HotkeyAction action) {
            dispatchHotkeyAction(action);
        });
    Bind(wxEVT_HOTKEY, &MainFrame::onHotkey, this);

    // ---- 1.5 create ThreadManager: panels need its reference at construction, so it must come first ----
    // it holds a reference to m_timer, so it must come after createTimer()
    m_threadMgr = std::make_unique<ThreadManager>(m_timer, *this);

    // ---- 2. assemble the UI ----
    m_notebook = new wxNotebook(this, wxID_ANY);

    // tab 1: mouse clicker
    m_clickerPanel = new ClickerPanel(m_notebook, *m_threadMgr);
    m_notebook->AddPage(m_clickerPanel, TR(IDS_TAB_CLICKER));

    // tab 2: keyboard simulator
    m_keyboardPanel = new KeyboardPanel(m_notebook, *m_threadMgr, m_timer);
    m_notebook->AddPage(m_keyboardPanel, TR(IDS_TAB_KEYBOARD));

    // tab 3: recording and sequencing
    m_recorderPanel = new RecorderPanel(m_notebook, *m_threadMgr);
    m_notebook->AddPage(m_recorderPanel, TR(IDS_TAB_RECORDER));

    // tab 4: settings (HotkeyManager injected)
    m_settingsPanel = new SettingsPanel(m_notebook, *m_hotkeyMgr);
    m_notebook->AddPage(m_settingsPanel, TR(IDS_TAB_SETTINGS));

    // tab 5: about
    m_aboutPanel = new AboutPanel(m_notebook);
    m_notebook->AddPage(m_aboutPanel, TR(IDS_TAB_ABOUT));

    // the notebook must be managed by a sizer, otherwise the frame never stretches it:
    // resize the window larger and it stays put; resize smaller and it gets clipped (the old bug)
    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(m_notebook, 1, wxEXPAND);
    SetSizer(root);

    // ---- 3. restore the last selected tab ----
    const int savedTab = ConfigManager::instance().config().tabIndex;
    if (savedTab >= 0 && savedTab < m_notebook->GetPageCount()) {
        m_notebook->SetSelection(savedTab);
    }

    wxWindowBase::SetBackgroundColour(wxColour(240, 240, 240));

    // ---- 4. status bar ----
    m_statusBar = new KBMacroStatusBar(this);
    wxFrameBase::SetStatusBar(m_statusBar);
    m_statusBar->SetMessage(TR(IDS_STATUS_READY));

    // ---- 5. settle initial geometry from content and screen (must be after panels + SetSizer) ----
    applyInitialGeometry(this);

    // ---- 6. maximize if the config says "maximized" ----
    if (ConfigManager::instance().config().window.maximized) {
        wxTopLevelWindowMSW::Maximize(true);
    }

    // ---- 7. register hotkeys last (panels are built, so later callbacks dispatch safely) ----
    m_hotkeyMgr->registerAll();
    reportHotkeyFailures();

    // ---- 8. the status timer drives the status bar's three states: task state lives only in ThreadManager,
    //        panels refresh their own buttons, the main window polls and aggregates (<=300ms late, decoupled)
    Bind(wxEVT_TIMER, &MainFrame::onStatusTimer, this);
    m_statusTimer.Start(300);

    // ---- 9. save geometry + tab on close ----
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::onClose, this);
}

MainFrame::~MainFrame() {
    // stop all tasks first (blocking join), then clear notification events still queued.
    // otherwise events CallAfter'ed before the worker exits stay alive until ~wxEvtHandler,
    // by which time panels are long gone and the panel pointers captured in the lambda dangle.
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
// wxEVT_HOTKEY handler: forward to HotkeyManager
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
// save window geometry + current tab
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void MainFrame::saveWindowState() {
    auto &cfgMgr = ConfigManager::instance();

    // ---- window geometry ----
    // patch the existing Config::Window instead of creating a new one: this way "while maximized,
    // x/y/width/height are untouched" holds naturally (keeps the values from the last normal state).
    Config::Window w = cfgMgr.config().window;

    if (IsMaximized()) {
        // maximized: update only the flag, geometry keeps the values from the last "normal" state
        w.maximized = true;
    } else if (IsIconized()) {
        // minimized: geometry is unreliable, change nothing (keep the previous values)
        // leave the flag alone too, keeping the previous choice
    } else {
        // normal state: update geometry + clear the maximized flag
        w.maximized = false;
        const wxPoint pos = GetPosition();
        const wxSize size = GetSize();
        w.x = pos.x;
        w.y = pos.y;
        w.width = size.x;
        w.height = size.y;
    }
    cfgMgr.setWindow(w);

    // ---- tab ----
    if (m_notebook) {
        cfgMgr.setTabIndex(m_notebook->GetSelection());
    }
}

// ============================================================
// wxEVT_CLOSE_WINDOW: save window geometry + current tab
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void MainFrame::onClose(wxCloseEvent &event) {
    // logged before saveWindowState: what may really stall on close is the later
    // ~MainFrame -> ThreadManager::StopAll (4 blocking joins), not saving the config
    LOG_INFO("mainframe: closing");
    saveWindowState();

    // let the close proceed
    event.Skip();
}

// ============================================================
// hotkey dispatch: call the matching panel's public method by action
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

        // "start all": clicker and keyboard are independent and can run together;
        // recording and playback are mutually exclusive, not auto-started here: the user picks one.
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
// summarize registration failures to the status bar
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
// status bar: three states = ready / running (lists the running tasks)
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
