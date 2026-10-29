#include "ui/MainFrame.hpp"

#include <wx/display.h>
#include <wx/notebook.h>

#include <algorithm>
#include <windows.h>

#include <windows.h>

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

    // 初始几何 = max(配置值, 内容 best size)，再夹到"窗口将要放的那个显示器"
    // 的可用区（已扣任务栏）的 90%。
    //
    // 必须在五个面板建好、SetSizer 之后调用 —— 那时 GetBestSize() 才是完整的。
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

        // 内容实际需要多少（已含当前 DPI 下的字体尺寸）。
        // GetBestSize() 是**客户区**尺寸，而 saveWindowState() 存的是 GetSize()
        // 的**窗口**尺寸；这里统一换成窗口尺寸再比较，否则每次启动都会
        // 把非客户区（标题栏 + 边框 + 状态栏）重复算进高度，窗口逐次变大。
        const wxSize content = frame->ClientToWindowSize(frame->GetBestSize());

        // 配置值兜底（用户手动调过就尊重），但不能小于内容、也不能超出屏幕
        wxSize target(std::max(w.width, std::min(content.x, maxW)),
                      std::max(w.height, std::min(content.y, maxH)));
        target.x = std::min(target.x, maxW);
        target.y = std::min(target.y, maxH);

        frame->SetSize(target);

        if (pos == wxDefaultPosition) {
            frame->Centre();
        } else {
            frame->Move(pos);
        }

        LOG_DEBUG("mainframe: content {}x{}, display {}x{}, window {}x{}",
                  content.x, content.y, area.width, area.height, target.x, target.y);
    }

} // namespace

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, TR(IDS_APP_TITLE),
              wxDefaultPosition, wxDefaultSize, initialStyle()) {
    LOG_INFO("mainframe: creating");
    // ---- 1. 先创建 HotkeyManager（只依赖 HWND）----
    m_hotkeyMgr = std::make_unique<HotkeyManager>(
        GetHWND(),
        [this](const HotkeyAction action) {
            dispatchHotkeyAction(action);
        });
    Bind(wxEVT_HOTKEY, &MainFrame::onHotkey, this);

    // ---- 2. 组装 UI ----
    m_notebook = new wxNotebook(this, wxID_ANY);

    // 标签页1：鼠标连点器
    m_clickerPanel = new ClickerPanel(m_notebook);
    m_notebook->AddPage(m_clickerPanel, TR(IDS_TAB_CLICKER));

    // 标签页2：键盘模拟器
    m_keyboardPanel = new KeyboardPanel(m_notebook);
    m_notebook->AddPage(m_keyboardPanel, TR(IDS_TAB_KEYBOARD));

    // 标签页3：录制与编排
    m_recorderPanel = new RecorderPanel(m_notebook);
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

    // ---- 8. 关闭时保存几何 + 标签页 ----
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::onClose, this);
}

MainFrame::~MainFrame() = default;

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
    m_statusBar->SetMessage(msg);
    LOG_WARN("mainframe: {} hotkey(s) failed to register", failed);
}
