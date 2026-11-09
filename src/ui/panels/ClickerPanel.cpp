#include <filesystem>

#include <wx/spinctrl.h>
#include <wx/statbox.h>

#include "common/Utils.hpp"
#include "common/JsonIO.hpp"
#include "common/Log.hpp"
#include "managers/LanguageManager.hpp"
#include "managers/ThreadManager.hpp"
#include "ui/panels/ClickerPanel.hpp"
#include "ui/ScreenCaptureOverlay.hpp"

namespace {
    // 点击类型下拉框的索引：滚轮类的范围
    constexpr int kTypeWheelFirst = 4;  // Wheel Up
    constexpr int kTypeWheelLast  = 7;  // Wheel Right

    // 运行模式下拉框索引
    constexpr int kRunModeInfinite = 0;
    constexpr int kRunModeTimed    = 1;
    constexpr int kRunModeCounted  = 2;

    // *.clicker.json 的 schema 版本
    constexpr int kClickerParamsVersion = 1;
}

ClickerPanel::ClickerPanel(wxWindow *parent, ThreadManager &threadMgr)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                   wxVSCROLL | wxHSCROLL),
      m_threadMgr(threadMgr) {
    buildUi();
    setRunning(false);

    // 页面级滚动：窗口装不下时才出现，正常尺寸下看不到滚动条
    SetScrollRate(FromDIP(10), FromDIP(10));
    wxWindowBase::FitInside();
}

void ClickerPanel::buildUi() {
    auto *root = new wxBoxSizer(wxVERTICAL);

    // ---- 分组一：坐标 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_CLICKER_GROUP_POS));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
        form->AddGrowableCol(1, 1);

        m_x = new wxTextCtrl(staticBox, wxID_ANY, "0");
        m_y = new wxTextCtrl(staticBox, wxID_ANY, "0");
        m_btnCapture = new wxButton(staticBox, wxID_ANY, TR(IDS_CLICKER_BTN_CAPTURE));

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_CLICKER_LABEL_X)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_x, 1, wxEXPAND);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_CLICKER_LABEL_Y)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_y, 1, wxEXPAND);

        form->AddSpacer(0);
        form->Add(m_btnCapture, 0);

        box->Add(form, 0, wxEXPAND | wxALL, 8);

        m_chkIgnorePos = new wxCheckBox(staticBox, wxID_ANY, TR(IDS_CLICKER_CHK_IGNORE_POS));
        box->Add(m_chkIgnorePos, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

        root->Add(box, 0, wxEXPAND | wxALL, 8);
    }

    // ---- 分组二：点击设置 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_CLICKER_GROUP_CLICK));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
        form->AddGrowableCol(1, 1);

        m_choiceType = new wxChoice(staticBox, wxID_ANY);
        m_choiceType->Append(TR(IDS_CLICKER_TYPE_LEFT));
        m_choiceType->Append(TR(IDS_CLICKER_TYPE_RIGHT));
        m_choiceType->Append(TR(IDS_CLICKER_TYPE_MIDDLE));
        m_choiceType->Append(TR(IDS_CLICKER_TYPE_DOUBLE));
        m_choiceType->Append(TR(IDS_CLICKER_TYPE_WHEEL_UP));
        m_choiceType->Append(TR(IDS_CLICKER_TYPE_WHEEL_DOWN));
        m_choiceType->Append(TR(IDS_CLICKER_TYPE_WHEEL_LEFT));
        m_choiceType->Append(TR(IDS_CLICKER_TYPE_WHEEL_RIGHT));
        m_choiceType->Append(TR(IDS_CLICKER_TYPE_SIDE1));
        m_choiceType->Append(TR(IDS_CLICKER_TYPE_SIDE2));
        m_choiceType->SetSelection(0);

        m_spinWheelSteps = new wxSpinCtrl(staticBox, wxID_ANY, wxEmptyString,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxSP_ARROW_KEYS, 1, 10, 1);
        m_spinInterval = new wxSpinCtrl(staticBox, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 10, 60000, 100);
        m_spinHoldTime = new wxSpinCtrl(staticBox, wxID_ANY, wxEmptyString,
                                        wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 5, 200, 50);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_CLICKER_LABEL_TYPE)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_choiceType, 1, wxEXPAND);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_CLICKER_LABEL_WHEEL_STEPS)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_spinWheelSteps, 1, wxEXPAND);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_CLICKER_LABEL_INTERVAL)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_spinInterval, 1, wxEXPAND);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_CLICKER_LABEL_HOLD)),
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

        m_btnStart = new wxButton(staticBox, wxID_ANY, TR(IDS_COMMON_BTN_START));
        m_btnStop = new wxButton(staticBox, wxID_ANY, TR(IDS_COMMON_BTN_STOP));
        m_btnImport = new wxButton(staticBox, wxID_ANY, TR(IDS_COMMON_BTN_IMPORT));
        m_btnExport = new wxButton(staticBox, wxID_ANY, TR(IDS_COMMON_BTN_EXPORT));

        box->Add(m_btnStart, 0, wxALL, 4);
        box->Add(m_btnStop, 0, wxALL, 4);
        box->AddStretchSpacer(1);
        box->Add(m_btnImport, 0, wxALL, 4);
        box->Add(m_btnExport, 0, wxALL, 4);

        root->Add(box, 0, wxEXPAND | wxALL, 8);
    }

    SetSizer(root);

    // ---- 事件绑定 ----
    m_btnCapture->Bind(wxEVT_BUTTON, &ClickerPanel::onCapturePos, this);
    m_chkIgnorePos->Bind(wxEVT_CHECKBOX, &ClickerPanel::onIgnorePos, this);
    m_choiceType->Bind(wxEVT_CHOICE, &ClickerPanel::onTypeChanged, this);
    m_choiceRunMode->Bind(wxEVT_CHOICE, &ClickerPanel::onRunModeChanged, this);
    m_btnStart->Bind(wxEVT_BUTTON, &ClickerPanel::onStart, this);
    m_btnStop->Bind(wxEVT_BUTTON, &ClickerPanel::onStop, this);
    m_btnImport->Bind(wxEVT_BUTTON, &ClickerPanel::onImport, this);
    m_btnExport->Bind(wxEVT_BUTTON, &ClickerPanel::onExport, this);

    // X/Y 只允许数字和负号
    m_x->Bind(wxEVT_CHAR, &ClickerPanel::onCoordChar, this);
    m_y->Bind(wxEVT_CHAR, &ClickerPanel::onCoordChar, this);

    // 初始状态刷新
    refreshEnableStates();
}

// ============================================================
// 参数收集 / 应用
// ============================================================

ClickerParams ClickerPanel::collectParams() const {
    ClickerParams p;

    p.ignorePosition = m_chkIgnorePos->GetValue();

    // 坐标用 ToLong 解析，失败时保留 0
    long coord = 0;
    if (m_x->GetValue().ToLong(&coord)) p.x = static_cast<int>(coord);
    if (m_y->GetValue().ToLong(&coord)) p.y = static_cast<int>(coord);

    p.clickType = static_cast<ClickType>(m_choiceType->GetSelection());
    p.wheelSteps = m_spinWheelSteps->GetValue();
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

void ClickerPanel::applyParams(const ClickerParams &p) {
    m_chkIgnorePos->SetValue(p.ignorePosition);

    m_x->SetValue(wxString::Format("%d", p.x));
    m_y->SetValue(wxString::Format("%d", p.y));

    m_choiceType->SetSelection(static_cast<int>(p.clickType));
    m_spinWheelSteps->SetValue(p.wheelSteps);
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

    refreshEnableStates();
}

// ============================================================
// 事件处理
// ============================================================

void ClickerPanel::onCapturePos(wxCommandEvent &) {
    LOG_DEBUG("clicker: capture position started");

    new ScreenCaptureOverlay(this,
        [this](const bool captured, const wxPoint pos) {
            if (!captured) {
                LOG_DEBUG("clicker: capture position cancelled"); // NOLINT(*-lambda-function-name)
                return;
            }
            LOG_DEBUG("clicker: captured position ({}, {})", pos.x, pos.y); // NOLINT(*-lambda-function-name)
            m_x->SetValue(wxString::Format("%d", pos.x));
            m_y->SetValue(wxString::Format("%d", pos.y));
        });

    refreshEnableStates();
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void ClickerPanel::onIgnorePos(wxCommandEvent &event) {
    LOG_DEBUG("clicker: ignore position = {}", event.IsChecked());
    refreshEnableStates();
}

void ClickerPanel::onTypeChanged(wxCommandEvent &) {
    refreshEnableStates();
}

void ClickerPanel::onRunModeChanged(wxCommandEvent &) {
    refreshEnableStates();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void ClickerPanel::onCoordChar(wxKeyEvent &event) { // NOLINT(*-convert-member-functions-to-static)
    const int key = event.GetKeyCode();

    // 控制键放行
    if (key == WXK_BACK || key == WXK_DELETE ||
        key == WXK_LEFT || key == WXK_RIGHT ||
        key == WXK_HOME || key == WXK_END || key == WXK_TAB) {
        event.Skip();
        return;
        }

    // 数字和负号放行
    if ((key >= '0' && key <= '9') || key == '-') {
        event.Skip();
        // ReSharper disable once CppRedundantControlFlowJump
        return;
    }

    // 其余吞掉
}

void ClickerPanel::onStart(wxCommandEvent &) {
    startWithCurrentParams();
}

void ClickerPanel::onStop(wxCommandEvent &) {
    stop();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void ClickerPanel::startWithCurrentParams() {
    const ClickerParams p = collectParams();

    LOG_INFO("clicker: start requested (interval={}ms, type={}, run={})",
             p.intervalMs, static_cast<int>(p.clickType),
             runModeToString(p.run.mode));

    // 已在跑时 ThreadManager 会忽略这次请求（同一任务不允许并发），
    // 所以不看返回值，直接按"已启动"切按钮；真结束时回调会再切回来
    m_threadMgr.StartClicker(p, [this](const TaskResult res) {
        setRunning(false);
        LOG_INFO("clicker: finished, reason={}, executed={}", // NOLINT(*-lambda-function-name)
                 static_cast<int>(res.reason), res.executed);
    });
    setRunning(true);
}

void ClickerPanel::stop() {
    LOG_INFO("clicker: stop requested");
    m_threadMgr.StopClicker();
    setRunning(false);
}

void ClickerPanel::onImport(wxCommandEvent &) {
    wxFileDialog dlg(this,
                     TR(IDS_DIALOG_IMPORT_PARAMS),
                     wxEmptyString,
                     wxEmptyString,
                     TR(IDS_FILTER_CLICKER),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    const auto path = std::filesystem::path(dlg.GetPath().ToStdWstring());

    nlohmann::json j;
    if (!jsonio::loadFrom(path, j)) {
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    // 版本校验
    if (const int fileVer = j.value("version", 1);
        fileVer > kClickerParamsVersion) {
        LOG_WARN("clicker: import version {} > supported {}",
                 fileVer, kClickerParamsVersion);
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    if (!j.contains("params") || !j["params"].is_object()) {
        LOG_WARN("clicker: import missing 'params' object");
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    try {
        applyParams(j["params"].get<ClickerParams>());
    } catch (const std::exception &e) {
        LOG_WARN("clicker: import deserialize failed: {}", e.what());
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    wxMessageBox(TR(IDS_MSG_IMPORT_SUCCESS), TR(IDS_APP_TITLE),
                 wxOK | wxICON_INFORMATION, this);
    LOG_INFO("clicker: params imported from {}",
             utils::ToUtf8(path.wstring()));
}

void ClickerPanel::onExport(wxCommandEvent &) {
    wxFileDialog dlg(this,
                     TR(IDS_DIALOG_EXPORT_PARAMS),
                     wxEmptyString,
                     L"clicker.clicker.json",
                     TR(IDS_FILTER_CLICKER),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    const auto path = std::filesystem::path(dlg.GetPath().ToStdWstring());

    nlohmann::json j;
    j["version"] = kClickerParamsVersion;
    j["params"] = collectParams();

    if (!jsonio::saveTo(path, j)) {
        wxMessageBox(TR(IDS_MSG_EXPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    wxMessageBox(TR(IDS_MSG_EXPORT_SUCCESS), TR(IDS_APP_TITLE),
                 wxOK | wxICON_INFORMATION, this);
    LOG_INFO("clicker: params exported to {}",
             utils::ToUtf8(path.wstring()));
}

bool ClickerPanel::isWheelTypeSelected() const {
    const int sel = m_choiceType->GetSelection();
    return sel >= kTypeWheelFirst && sel <= kTypeWheelLast;
}

// ReSharper disable once CppMemberFunctionMayBeConst
void ClickerPanel::refreshEnableStates() {
    // ---- 坐标区 ----
    const bool ignorePos = m_chkIgnorePos->GetValue();
    m_x->Enable(!ignorePos);
    m_y->Enable(!ignorePos);
    m_btnCapture->Enable(!ignorePos);

    // ---- 点击设置 ----
    const bool isWheel = isWheelTypeSelected();
    m_spinWheelSteps->Enable(isWheel);
    m_spinHoldTime->Enable(!isWheel);

    // ---- 运行模式 ----
    const int runMode = m_choiceRunMode->GetSelection();
    m_spinDuration->Enable(runMode == kRunModeTimed);
    m_spinCount->Enable(runMode == kRunModeCounted);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void ClickerPanel::setRunning(const bool running) {
    // 运行中改参数不会影响已启动的任务（参数在 StartClicker 时已快照进闭包），
    // 所以这里只切按钮，不去禁用输入控件
    if (m_btnStart != nullptr) m_btnStart->Enable(!running);
    if (m_btnStop != nullptr) m_btnStop->Enable(running);
}
