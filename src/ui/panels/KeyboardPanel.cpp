#include <filesystem>

#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <stop_token>

#include "common/Utils.hpp"
#include "common/Hotkey.hpp"
#include "common/JsonIO.hpp"
#include "common/Log.hpp"

#include "win32/Input.hpp"
#include "managers/ThreadManager.hpp"
#include "managers/LanguageManager.hpp"
#include "ui/panels/KeyboardPanel.hpp"
#include "ui/dialogs/HotkeyCaptureDialog.hpp"

namespace {
    // 运行模式下拉框索引
    constexpr int kRunModeInfinite = 0;
    constexpr int kRunModeTimed    = 1;
    constexpr int kRunModeCounted  = 2;

    // *.keyboard.json 的 schema 版本
    constexpr int kKeyboardParamsVersion = 1;
}

KeyboardPanel::KeyboardPanel(wxWindow *parent, ThreadManager &threadMgr,
                             std::shared_ptr<ITimer> &timerRef)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL),
      m_threadMgr(threadMgr), m_timerRef(timerRef) {
    buildUi();
    setRunning(false);

    // 页面级滚动：窗口装不下时才出现，正常尺寸下看不到滚动条
    SetScrollRate(0, FromDIP(10));
    FitInside();
}

void KeyboardPanel::buildUi() {
    auto *root = new wxBoxSizer(wxVERTICAL);

    // ---- 分组一：按键 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_KEYBOARD_GROUP_KEY));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
        form->AddGrowableCol(1, 1);

        m_keyDisplay = new wxTextCtrl(staticBox, wxID_ANY,
                                      TR(IDS_KEYBOARD_KEY_UNSET),
                                      wxDefaultPosition, wxDefaultSize,
                                      wxTE_READONLY);
        m_btnSetKey = new wxButton(staticBox, wxID_ANY, TR(IDS_KEYBOARD_BTN_SET_KEY));

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_KEYBOARD_LABEL_KEY)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_keyDisplay, 1, wxEXPAND);

        form->AddSpacer(0);
        form->Add(m_btnSetKey, 0);

        box->Add(form, 0, wxEXPAND | wxALL, 8);

        root->Add(box, 0, wxEXPAND | wxALL, 8);
    }

    // ---- 分组二：参数 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_KEYBOARD_GROUP_PARAMS));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
        form->AddGrowableCol(1, 1);

        m_spinInterval = new wxSpinCtrl(staticBox, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 10, 60000, 100);
        m_spinHoldTime = new wxSpinCtrl(staticBox, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 5, 200, 50);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_KEYBOARD_LABEL_INTERVAL)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_spinInterval, 1, wxEXPAND);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_KEYBOARD_LABEL_HOLD)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_spinHoldTime, 1, wxEXPAND);

        box->Add(form, 0, wxEXPAND | wxALL, 8);

        root->Add(box, 0, wxEXPAND | wxALL, 8);
    }

    // ---- 分组三：运行模式 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_COMMON_GROUP_RUNMODE));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
        form->AddGrowableCol(1, 1);

        m_choiceRunMode = new wxChoice(staticBox, wxID_ANY);
        m_choiceRunMode->Append(TR(IDS_COMMON_RUNMODE_INFINITE));
        m_choiceRunMode->Append(TR(IDS_COMMON_RUNMODE_TIMED));
        m_choiceRunMode->Append(TR(IDS_COMMON_RUNMODE_COUNTED));
        m_choiceRunMode->SetSelection(0);

        m_spinDuration = new wxSpinCtrl(staticBox, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 1, 86400, 10);
        m_spinCount = new wxSpinCtrl(staticBox, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 1, 999999, 100);
        m_spinStartDelay = new wxSpinCtrl(staticBox, wxID_ANY, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxSP_ARROW_KEYS, 0, 5000, 0);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_COMMON_LABEL_RUNMODE)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_choiceRunMode, 1, wxEXPAND);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_COMMON_LABEL_DURATION)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_spinDuration, 1, wxEXPAND);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_COMMON_LABEL_COUNT)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_spinCount, 1, wxEXPAND);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_COMMON_LABEL_START_DELAY)),
          0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_spinStartDelay, 1, wxEXPAND);

        box->Add(form, 0, wxEXPAND | wxALL, 8);

        root->Add(box, 0, wxEXPAND | wxALL, 8);
    }

    // ---- 分组四：操作 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_COMMON_GROUP_ACTIONS));
        auto *box = new wxStaticBoxSizer(staticBox, wxHORIZONTAL);

        m_btnSendOnce = new wxButton(staticBox, wxID_ANY, TR(IDS_KEYBOARD_BTN_SEND_ONCE));
        m_btnStart = new wxButton(staticBox, wxID_ANY, TR(IDS_COMMON_BTN_START));
        m_btnStop = new wxButton(staticBox, wxID_ANY, TR(IDS_COMMON_BTN_STOP));
        m_btnImport = new wxButton(staticBox, wxID_ANY, TR(IDS_COMMON_BTN_IMPORT));
        m_btnExport = new wxButton(staticBox, wxID_ANY, TR(IDS_COMMON_BTN_EXPORT));

        box->Add(m_btnSendOnce, 0, wxALL, 4);
        box->Add(m_btnStart, 0, wxALL, 4);
        box->Add(m_btnStop, 0, wxALL, 4);
        box->AddStretchSpacer(1);
        box->Add(m_btnImport, 0, wxALL, 4);
        box->Add(m_btnExport, 0, wxALL, 4);

        root->Add(box, 0, wxEXPAND | wxALL, 8);
    }

    SetSizer(root);

    // ---- 事件绑定 ----
    m_btnSetKey->Bind(wxEVT_BUTTON, &KeyboardPanel::onSetKey, this);
    m_btnSendOnce->Bind(wxEVT_BUTTON, &KeyboardPanel::onSendOnce, this);
    m_btnStart->Bind(wxEVT_BUTTON, &KeyboardPanel::onStart, this);
    m_btnStop->Bind(wxEVT_BUTTON, &KeyboardPanel::onStop, this);
    m_btnImport->Bind(wxEVT_BUTTON, &KeyboardPanel::onImport, this);
    m_btnExport->Bind(wxEVT_BUTTON, &KeyboardPanel::onExport, this);
    m_choiceRunMode->Bind(wxEVT_CHOICE, &KeyboardPanel::onRunModeChanged, this);

    refreshKeyDisplay();
    refreshEnableStates();
}

// ============================================================
// 参数收集 / 应用
// ============================================================

KeyboardParams KeyboardPanel::collectParams() const {
    KeyboardParams p;
    p.hotkey = m_hotkey;
    p.intervalMs = m_spinInterval->GetValue();
    p.holdMs = m_spinHoldTime->GetValue();

    // ReSharper disable once CppDeclaratorNeverUsed
    switch (const int runSel = m_choiceRunMode->GetSelection()) {
        case kRunModeTimed:   p.run.mode = RunMode::Timed;   break;
        case kRunModeCounted: p.run.mode = RunMode::Counted; break;
        default:              p.run.mode = RunMode::Infinite; break;
    }
    p.run.durationSec = m_spinDuration->GetValue();
    p.run.count = m_spinCount->GetValue();
    p.run.startDelayMs = m_spinStartDelay->GetValue();

    return p;
}

void KeyboardPanel::applyParams(const KeyboardParams &p) {
    m_hotkey = p.hotkey;
    m_spinInterval->SetValue(p.intervalMs);
    m_spinHoldTime->SetValue(p.holdMs);

    switch (p.run.mode) {
        case RunMode::Timed:   m_choiceRunMode->SetSelection(kRunModeTimed);   break;
        case RunMode::Counted: m_choiceRunMode->SetSelection(kRunModeCounted); break;
        default:               m_choiceRunMode->SetSelection(kRunModeInfinite); break;
    }
    m_spinDuration->SetValue(p.run.durationSec);
    m_spinCount->SetValue(p.run.count);
    m_spinStartDelay->SetValue(p.run.startDelayMs);

    refreshKeyDisplay();
    refreshEnableStates();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void KeyboardPanel::refreshKeyDisplay() {
    const std::wstring s = utils::hotkeyToString(m_hotkey);
    if (s.empty()) {
        m_keyDisplay->SetValue(TR(IDS_KEYBOARD_KEY_UNSET));
    } else {
        m_keyDisplay->SetValue(s);
    }
}

// ============================================================
// 事件处理
// ============================================================

void KeyboardPanel::onSetKey(wxCommandEvent &) {
    HotkeyCaptureDialog dlg(this,
                            TR(IDS_KEYBOARD_BTN_SET_KEY),
                            m_hotkey,
                            /*requireMainKey=*/false);
    if (dlg.ShowModal() != wxID_OK) return;

    m_hotkey = dlg.getHotkey();
    refreshKeyDisplay();

    LOG_DEBUG("keyboard: hotkey set to {}",
              utils::ToUtf8(utils::hotkeyToString(m_hotkey)));
}

void KeyboardPanel::onSendOnce(wxCommandEvent &) {
    sendOnce();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void KeyboardPanel::sendOnce() {
    const KeyboardParams p = collectParams();

    LOG_INFO("keyboard: send once requested (key='{}')",
             utils::ToUtf8(utils::hotkeyToString(p.hotkey)));

    if (p.hotkey.vk == 0 && p.hotkey.modifiers == 0) {
        LOG_WARN("keyboard: send once ignored, no key configured");
        return;
    }

    // 一次性发送不走线程、不经过 ThreadManager，没有异常接收方，
    // 所以必须自己兜住：让 SendInputError 穿透 wx 事件循环会直接 terminate
    try {
        // 空 stop_token：一次性发送没有"中途停止"的语义，必定等满 holdMs
        static_cast<void>(input::pressKey(p.hotkey, p.holdMs, *m_timerRef, std::stop_token{}));
    } catch (const std::exception &e) {
        LOG_ERROR("keyboard: send once failed: {}", e.what());
    }
}

void KeyboardPanel::onStart(wxCommandEvent &) {
    startWithCurrentParams();
}

void KeyboardPanel::onStop(wxCommandEvent &) {
    stop();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void KeyboardPanel::startWithCurrentParams() {
    const KeyboardParams p = collectParams();

    if (p.hotkey.vk == 0 && p.hotkey.modifiers == 0) {
        // TODO(接线): 换成状态栏提示 + i18n 资源
        LOG_WARN("keyboard: start ignored, no key configured");
        return;
    }

    LOG_INFO("keyboard: start requested (interval={}ms, key='{}', run={})",
             p.intervalMs,
             utils::ToUtf8(utils::hotkeyToString(p.hotkey)),
             runModeToString(p.run.mode));

    // 已在跑时 ThreadManager 会忽略这次请求，所以不看返回值直接切按钮
    m_threadMgr.StartKeyboard(p, [this](const TaskResult res) {
        setRunning(false);
        LOG_INFO("keyboard: finished, reason={}, executed={}",
                 static_cast<int>(res.reason), res.executed);
    });
    setRunning(true);
}

void KeyboardPanel::stop() {
    LOG_INFO("keyboard: stop requested");
    m_threadMgr.StopKeyboard();
    setRunning(false);
}

void KeyboardPanel::onImport(wxCommandEvent &) {
    wxFileDialog dlg(this,
                     TR(IDS_DIALOG_IMPORT_PARAMS),
                     wxEmptyString,
                     wxEmptyString,
                     TR(IDS_FILTER_KEYBOARD),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    const auto path = std::filesystem::path(dlg.GetPath().ToStdWstring());

    nlohmann::json j;
    if (!jsonio::loadFrom(path, j)) {
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    if (const int fileVer = j.value("version", 1);
        fileVer > kKeyboardParamsVersion) {
        LOG_WARN("keyboard: import version {} > supported {}",
                 fileVer, kKeyboardParamsVersion);
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    if (!j.contains("params") || !j["params"].is_object()) {
        LOG_WARN("keyboard: import missing 'params' object");
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    try {
        applyParams(j["params"].get<KeyboardParams>());
    } catch (const std::exception &e) {
        LOG_WARN("keyboard: import deserialize failed: {}", e.what());
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    wxMessageBox(TR(IDS_MSG_IMPORT_SUCCESS), TR(IDS_APP_TITLE),
                 wxOK | wxICON_INFORMATION, this);
    LOG_INFO("keyboard: params imported from {}",
             utils::ToUtf8(path.wstring()));
}

void KeyboardPanel::onExport(wxCommandEvent &) {
    wxFileDialog dlg(this,
                     TR(IDS_DIALOG_EXPORT_PARAMS),
                     wxEmptyString,
                     L"keyboard.keyboard.json",
                     TR(IDS_FILTER_KEYBOARD),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    const auto path = std::filesystem::path(dlg.GetPath().ToStdWstring());

    nlohmann::json j;
    j["version"] = kKeyboardParamsVersion;
    j["params"] = collectParams();

    if (!jsonio::saveTo(path, j)) {
        wxMessageBox(TR(IDS_MSG_EXPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    wxMessageBox(TR(IDS_MSG_EXPORT_SUCCESS), TR(IDS_APP_TITLE),
                 wxOK | wxICON_INFORMATION, this);
    LOG_INFO("keyboard: params exported to {}",
             utils::ToUtf8(path.wstring()));
}

void KeyboardPanel::onRunModeChanged(wxCommandEvent &) {
    refreshEnableStates();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void KeyboardPanel::refreshEnableStates() {
    const int runMode = m_choiceRunMode->GetSelection();
    m_spinDuration->Enable(runMode == kRunModeTimed);
    m_spinCount->Enable(runMode == kRunModeCounted);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void KeyboardPanel::setRunning(const bool running) {
    // 运行中改参数不影响已启动的任务（参数在 StartKeyboard 时已快照），
    // 只切按钮；"发送一次"是无状态的，运行中也允许点
    if (m_btnStart != nullptr) m_btnStart->Enable(!running);
    if (m_btnStop != nullptr) m_btnStop->Enable(running);
}
