#include "ui/panels/RecorderPanel.hpp"

#include <filesystem>
#include <utility>

#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/richmsgdlg.h>
#include <cassert>

#include "common/Utils.hpp"
#include "common/Hotkey.hpp"
#include "common/JsonIO.hpp"
#include "common/Log.hpp"
#include "common/CoordAdapt.hpp"
#include "managers/ConfigManager.hpp"
#include "managers/LanguageManager.hpp"
#include "ui/dialogs/ActionEditDialog.hpp"

namespace {

    // ---- Action → 显示文本 ----

    wxString actionTypeText(const ActionType t) {
        switch (t) {
            case ActionType::MouseMove:  return {TR(IDS_ACTION_TYPE_MOUSE_MOVE)};
            case ActionType::MouseClick: return {TR(IDS_ACTION_TYPE_MOUSE_CLICK)};
            case ActionType::MouseWheel: return {TR(IDS_ACTION_TYPE_MOUSE_WHEEL)};
            case ActionType::Keyboard:   return {TR(IDS_ACTION_TYPE_KEYBOARD)};
        }
        return {};
    }

    wxString mouseButtonText(const MouseButton b) {
        switch (b) {
            case MouseButton::Left:   return {TR(IDS_CLICKER_TYPE_LEFT)};
            case MouseButton::Right:  return {TR(IDS_CLICKER_TYPE_RIGHT)};
            case MouseButton::Middle: return {TR(IDS_CLICKER_TYPE_MIDDLE)};
            case MouseButton::Side1:  return {TR(IDS_CLICKER_TYPE_SIDE1)};
            case MouseButton::Side2:  return {TR(IDS_CLICKER_TYPE_SIDE2)};
        }
        return {};
    }

    wxString wheelDirText(const WheelDirection d) {
        switch (d) {
            case WheelDirection::Up:    return {TR(IDS_ACTION_WHEEL_UP)};
            case WheelDirection::Down:  return {TR(IDS_ACTION_WHEEL_DOWN)};
            case WheelDirection::Left:  return {TR(IDS_ACTION_WHEEL_LEFT)};
            case WheelDirection::Right: return {TR(IDS_ACTION_WHEEL_RIGHT)};
        }
        return {};
    }

    wxString actionParamText(const Action &a) {
        switch (a.type) {
            case ActionType::MouseMove:
                return wxString::Format("(%d, %d)", a.x, a.y);
            case ActionType::MouseClick:
                return wxString::Format("%s (%d, %d)",
                                        mouseButtonText(a.button), a.x, a.y);
            case ActionType::MouseWheel:
                return wxString::Format("%s x%d", wheelDirText(a.wheelDirection), a.wheelSteps);
            case ActionType::Keyboard: {
                const auto key = wxString(utils::hotkeyToString(a.hotkey));
                const auto evt = (a.keyEvent == KeyEventType::Down)
                                     ? wxString(TR(IDS_ACTION_KEY_DOWN))
                                     : wxString(TR(IDS_ACTION_KEY_UP));
                return wxString::Format("%s [%s]", key, evt);
            }
        }
        return {};
    }

    // 从 wxListCtrl 的 long 行号安全转 vector 索引。
    // 调用方保证 row >= 0（选中判断之后才调用）。
    [[nodiscard]] std::size_t toIndex(const long row) {
        assert(row >= 0);
        return static_cast<std::size_t>(row);
    }

    [[nodiscard]] long toRow(const std::size_t i) {
        return static_cast<long>(i);
    }

    // size_t → 有符号索引（用于迭代器算术）
    [[nodiscard]] std::ptrdiff_t toDiff(const std::size_t i) {
        return static_cast<std::ptrdiff_t>(i);
    }

    // *.macro.json 的 schema 版本
    constexpr int kMacroVersion = 1;
} // namespace

RecorderPanel::RecorderPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
    buildUi();
}

void RecorderPanel::buildUi() {
    auto *root = new wxBoxSizer(wxVERTICAL);

    // ---- 分组一：录制 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_RECORDER_GROUP_RECORD));
        auto *box = new wxStaticBoxSizer(staticBox, wxHORIZONTAL);

        m_btnStartRecord = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_START_RECORD));
        m_btnStopRecord = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_STOP_RECORD));

        box->Add(m_btnStartRecord, 0, wxALL, 4);
        box->Add(m_btnStopRecord, 0, wxALL, 4);
        box->AddStretchSpacer(1);

        root->Add(box, 0, wxEXPAND | wxALL, 8);
    }

    // ---- 分组二：动作列表 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_RECORDER_GROUP_ACTION_LIST));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        m_listCtrl = new wxListCtrl(staticBox, wxID_ANY,
                                    wxDefaultPosition, FromDIP(wxSize(-1, 200)),
                                    wxLC_REPORT | wxLC_SINGLE_SEL);
        m_listCtrl->AppendColumn(TR(IDS_RECORDER_LIST_COL_INDEX), wxLIST_FORMAT_RIGHT, FromDIP(40));
        m_listCtrl->AppendColumn(TR(IDS_RECORDER_LIST_COL_ACTION), wxLIST_FORMAT_LEFT, FromDIP(100));
        m_listCtrl->AppendColumn(TR(IDS_RECORDER_LIST_COL_PARAM), wxLIST_FORMAT_LEFT, FromDIP(140));
        m_listCtrl->AppendColumn(TR(IDS_RECORDER_LIST_COL_DELAY), wxLIST_FORMAT_RIGHT, FromDIP(80));
        m_listCtrl->AppendColumn(TR(IDS_RECORDER_LIST_COL_HOLD), wxLIST_FORMAT_RIGHT, FromDIP(80));

        box->Add(m_listCtrl, 0, wxEXPAND | wxALL, 8);

        auto *btnRow = new wxBoxSizer(wxHORIZONTAL);
        m_btnAdd = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_ADD));
        m_btnEdit = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_EDIT));
        m_btnDelete = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_DELETE));
        m_btnMoveUp = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_MOVE_UP));
        m_btnMoveDown = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_MOVE_DOWN));
        m_btnClear = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_CLEAR));

        btnRow->Add(m_btnAdd, 0, wxALL, 4);
        btnRow->Add(m_btnEdit, 0, wxALL, 4);
        btnRow->Add(m_btnDelete, 0, wxALL, 4);
        btnRow->Add(m_btnMoveUp, 0, wxALL, 4);
        btnRow->Add(m_btnMoveDown, 0, wxALL, 4);
        btnRow->AddStretchSpacer(1);
        btnRow->Add(m_btnClear, 0, wxALL, 4);

        box->Add(btnRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

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

    // ---- 分组四：回放 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_RECORDER_GROUP_PLAYBACK));
        auto *box = new wxStaticBoxSizer(staticBox, wxHORIZONTAL);

        m_btnStartPlayback = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_START_PLAYBACK));
        m_btnStopPlayback = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_STOP_PLAYBACK));
        m_btnImportScript = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_IMPORT_SCRIPT));
        m_btnExportScript = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_EXPORT_SCRIPT));

        box->Add(m_btnStartPlayback, 0, wxALL, 4);
        box->Add(m_btnStopPlayback, 0, wxALL, 4);
        box->AddStretchSpacer(1);
        box->Add(m_btnImportScript, 0, wxALL, 4);
        box->Add(m_btnExportScript, 0, wxALL, 4);

        root->Add(box, 0, wxEXPAND | wxALL, 8);
    }

    SetSizer(root);

    // ---- 事件绑定 ----
    m_btnStartRecord->Bind(wxEVT_BUTTON, &RecorderPanel::onStartRecord, this);
    m_btnStopRecord->Bind(wxEVT_BUTTON, &RecorderPanel::onStopRecord, this);
    m_btnAdd->Bind(wxEVT_BUTTON, &RecorderPanel::onAdd, this);
    m_btnEdit->Bind(wxEVT_BUTTON, &RecorderPanel::onEdit, this);
    m_btnDelete->Bind(wxEVT_BUTTON, &RecorderPanel::onDelete, this);
    m_btnMoveUp->Bind(wxEVT_BUTTON, &RecorderPanel::onMoveUp, this);
    m_btnMoveDown->Bind(wxEVT_BUTTON, &RecorderPanel::onMoveDown, this);
    m_btnClear->Bind(wxEVT_BUTTON, &RecorderPanel::onClear, this);
    m_btnStartPlayback->Bind(wxEVT_BUTTON, &RecorderPanel::onStartPlayback, this);
    m_btnStopPlayback->Bind(wxEVT_BUTTON, &RecorderPanel::onStopPlayback, this);
    m_btnImportScript->Bind(wxEVT_BUTTON, &RecorderPanel::onImportScript, this);
    m_btnExportScript->Bind(wxEVT_BUTTON, &RecorderPanel::onExportScript, this);

    m_listCtrl->Bind(wxEVT_LIST_ITEM_SELECTED,
                     &RecorderPanel::onListSelectionChanged, this);
    m_listCtrl->Bind(wxEVT_LIST_ITEM_DESELECTED,
                     &RecorderPanel::onListSelectionChanged, this);

    // ---- 初始填充 ----
    refreshList();

    // ---- 运行模式下拉框事件 ----
    m_choiceRunMode->Bind(wxEVT_CHOICE, &RecorderPanel::onRunModeChanged, this);
    refreshEnableStates();
}

// ============================================================
// 参数收集
// ============================================================

RunSpec RecorderPanel::collectRunSpec() const {
    RunSpec run;

    // ReSharper disable once CppDeclaratorNeverUsed
    switch (const int sel = m_choiceRunMode->GetSelection()) {
        case 1:  run.mode = RunMode::Timed;   break;
        case 2:  run.mode = RunMode::Counted; break;
        default: run.mode = RunMode::Infinite; break;
    }
    run.durationSec = m_spinDuration->GetValue();
    run.count = m_spinCount->GetValue();
    run.startDelayMs = m_spinStartDelay->GetValue();

    return run;
}

// ===========================================================
// 参数应用
// ===========================================================
void RecorderPanel::applyRunSpec(const RunSpec &run) {
    switch (run.mode) {
        case RunMode::Timed:   m_choiceRunMode->SetSelection(1); break;
        case RunMode::Counted: m_choiceRunMode->SetSelection(2); break;
        default:               m_choiceRunMode->SetSelection(0); break;
    }
    m_spinDuration->SetValue(run.durationSec);
    m_spinCount->SetValue(run.count);
    m_spinStartDelay->SetValue(run.startDelayMs);
    refreshEnableStates();
}

// ============================================================
// 列表维护
// ============================================================
void RecorderPanel::refreshList() {
    m_listCtrl->DeleteAllItems();

    for (std::size_t i = 0; i < m_actions.size(); ++i) {
        const auto &a = m_actions[i];
        const long row = m_listCtrl->InsertItem(static_cast<long>(i),
                                                wxString::Format("%zu", i + 1));
        m_listCtrl->SetItem(row, 1, actionTypeText(a.type));
        m_listCtrl->SetItem(row, 2, actionParamText(a));
        m_listCtrl->SetItem(row, 3, wxString::Format("%d", a.delayMs));
        if (a.type == ActionType::MouseClick) {
            m_listCtrl->SetItem(row, 4, wxString::Format("%d", a.holdMs));
        } else {
            m_listCtrl->SetItem(row, 4, "-");
        }
    }

    refreshButtonStates();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void RecorderPanel::refreshButtonStates() {
    const long row = getSelectedRow();
    const long total = m_listCtrl->GetItemCount();

    const bool hasSelection = (row >= 0);
    const bool hasItems = (total > 0);

    m_btnEdit->Enable(hasSelection);
    m_btnDelete->Enable(hasSelection);
    m_btnMoveUp->Enable(hasSelection && row > 0);
    m_btnMoveDown->Enable(hasSelection && row < total - 1);
    m_btnClear->Enable(hasItems);
}

void RecorderPanel::selectRow(const long row) {
    const long total = m_listCtrl->GetItemCount();
    if (row < 0 || row >= total) return;

    m_listCtrl->SetItemState(row, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    m_listCtrl->EnsureVisible(row);
    refreshButtonStates();
}

long RecorderPanel::getSelectedRow() const {
    return m_listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
}

void RecorderPanel::onListSelectionChanged(wxListEvent &event) {
    refreshButtonStates();
    event.Skip();
}

// ============================================================
// 列表操作
// ============================================================
void RecorderPanel::onAdd(wxCommandEvent &) {
    Action newAction; // 默认 MouseMove
    ActionEditDialog dlg(this, TR(IDS_ACTION_DIALOG_TITLE_ADD), newAction);
    if (dlg.ShowModal() != wxID_OK) return;

    m_actions.push_back(dlg.getAction());
    refreshList();
    selectRow(static_cast<long>(m_actions.size()) - 1);

    LOG_DEBUG("recorder: added action, total={}", m_actions.size());
}

void RecorderPanel::onEdit(wxCommandEvent &) {
    const long row = getSelectedRow();
    if (row < 0) return;

    ActionEditDialog dlg(this, TR(IDS_ACTION_DIALOG_TITLE_EDIT),
                         m_actions[toIndex(row)]);
    if (dlg.ShowModal() != wxID_OK) return;

    m_actions[toIndex(row)] = dlg.getAction();
    refreshList();
    selectRow(row);

    LOG_DEBUG("recorder: edited action at row {}", row);
}

void RecorderPanel::onDelete(wxCommandEvent &) {
    const long row = getSelectedRow();
    if (row < 0) return;

    m_actions.erase(m_actions.begin() + toDiff(row));
    refreshList();

    // 尽量选中同一位置，删了最后一个就往前退一格
    if (!m_actions.empty()) {
        const long target = std::min<long>(row, toRow(m_actions.size() - 1));
        selectRow(target);
    }

    LOG_DEBUG("recorder: deleted action, remaining={}", m_actions.size());
}

void RecorderPanel::onMoveUp(wxCommandEvent &) {
    const long row = getSelectedRow();
    if (row <= 0) return;

    std::swap(m_actions[toIndex(row)],
              m_actions[toIndex(row - 1)]);
    refreshList();
    selectRow(row - 1);

    LOG_DEBUG("recorder: moved action up from row {}", row);
}

void RecorderPanel::onMoveDown(wxCommandEvent &) {
    const long row = getSelectedRow();
    if (row < 0 || row >= static_cast<long>(m_actions.size()) - 1) return;

    std::swap(m_actions[toIndex(row)],
              m_actions[toIndex(row + 1)]);
    refreshList();
    selectRow(row + 1);

    LOG_DEBUG("recorder: moved action down from row {}", row);
}

void RecorderPanel::onClear(wxCommandEvent &) {
    if (m_actions.empty()) return;

    m_actions.clear();
    refreshList();

    LOG_DEBUG("recorder: cleared all actions");
}

// ============================================================
// 运行模式
// ============================================================

void RecorderPanel::onRunModeChanged(wxCommandEvent &) {
    refreshEnableStates();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void RecorderPanel::refreshEnableStates() {
    const int runMode = m_choiceRunMode->GetSelection();
    m_spinDuration->Enable(runMode == 1);
    m_spinCount->Enable(runMode == 2);
}


// ============================================================
// 录制 / 回放
// ============================================================
void RecorderPanel::onStartRecord(wxCommandEvent &) {
    startRecording();
}

void RecorderPanel::onStopRecord(wxCommandEvent &) {
    stopRecording();
}

void RecorderPanel::onStartPlayback(wxCommandEvent &) {
    startPlayback();
}

void RecorderPanel::onStopPlayback(wxCommandEvent &) {
    stopPlayback();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::startRecording() { // NOLINT(*-convert-member-functions-to-static)
    LOG_INFO("recorder: start recording requested");
    // TODO: ThreadManager::StartRecord()
}
// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::stopRecording() { // NOLINT(*-convert-member-functions-to-static)
    LOG_INFO("recorder: stop recording requested");
    // TODO: ThreadManager::StopRecord()
}

// ReSharper disable once CppMemberFunctionMayBeConst
void RecorderPanel::startPlayback() {
    const RunSpec run = collectRunSpec();
    LOG_INFO("recorder: start playback requested (actions={}, mode={})",
             m_actions.size(), runModeToString(run.mode));
    // TODO: ThreadManager::StartPlayback(m_actions, run)
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::stopPlayback() { // NOLINT(*-convert-member-functions-to-static)
    LOG_INFO("recorder: stop playback requested");
    // TODO: ThreadManager::StopPlayback()
}

// ============================================================
// 导入 / 导出脚本
// ============================================================

void RecorderPanel::onImportScript(wxCommandEvent &) {
    wxFileDialog dlg(this,
                     TR(IDS_DIALOG_IMPORT_SCRIPT),
                     wxEmptyString,
                     wxEmptyString,
                     TR(IDS_FILTER_MACRO),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    const auto path = std::filesystem::path(dlg.GetPath().ToStdWstring());

    nlohmann::json j;
    if (!jsonio::loadFrom(path, j)) {
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    if (const int fileVer = j.value("version", 1); fileVer > kMacroVersion) {
        LOG_WARN("recorder: import version {} > supported {}",
                 fileVer, kMacroVersion);
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    if (!j.contains("actions") || !j["actions"].is_array()) {
        LOG_WARN("recorder: import missing 'actions' array");
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    std::vector<Action> imported;
    try {
        imported = j["actions"].get<std::vector<Action>>();
    } catch (const std::exception &e) {
        LOG_WARN("recorder: import deserialize failed: {}", e.what());
        wxMessageBox(TR(IDS_MSG_IMPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    // ---- 屏幕规格比对 ----
    ScreenSpec scriptSpec;
    if (j.contains("screen") && j["screen"].is_object()) {
        scriptSpec = j["screen"].get<ScreenSpec>();
    }

    const ScreenSpec currentSpec = utils::GetPrimaryScreenSpec();
    const bool specsDiffer =
        scriptSpec.width > 0 && scriptSpec.height > 0 &&
        (scriptSpec.width != currentSpec.width ||
         scriptSpec.height != currentSpec.height ||
         scriptSpec.dpi != currentSpec.dpi);

    if (specsDiffer) {
        LOG_INFO("recorder: screen spec differs: script={}x{}@{}dpi, "
                 "current={}x{}@{}dpi",
                 scriptSpec.width, scriptSpec.height, scriptSpec.dpi,
                 currentSpec.width, currentSpec.height, currentSpec.dpi);

        const AdaptChoice choice = askAdaptCoordinates(scriptSpec, currentSpec);
        if (choice == AdaptChoice::Cancelled) {
            LOG_INFO("recorder: import cancelled at adapt prompt");
            return;
        }
        if (choice == AdaptChoice::Adapt) {
            coord::adaptActions(imported, scriptSpec, currentSpec);
            LOG_INFO("recorder: user chose to adapt coordinates");
        } else {
            LOG_INFO("recorder: user chose to keep original coordinates");
        }
    }

    // 替换确认
    const int ret = wxMessageBox(TR(IDS_MSG_MACRO_REPLACE_CONFIRM),
                                 TR(IDS_APP_TITLE),
                                 wxOK | wxCANCEL | wxICON_QUESTION, this);
    if (ret != wxOK) {
        LOG_DEBUG("recorder: import cancelled by user");
        return;
    }

    m_actions = std::move(imported);
    refreshList();

    // 运行模式：缺失走默认（Infinite）
    if (j.contains("run") && j["run"].is_object()) {
        applyRunSpec(j["run"].get<RunSpec>());
    }

    wxMessageBox(TR(IDS_MSG_IMPORT_SUCCESS), TR(IDS_APP_TITLE),
                 wxOK | wxICON_INFORMATION, this);
    LOG_INFO("recorder: imported {} actions from {}",
             m_actions.size(), utils::ToUtf8(path.wstring()));
}

void RecorderPanel::onExportScript(wxCommandEvent &) {
    wxFileDialog dlg(this,
                     TR(IDS_DIALOG_EXPORT_SCRIPT),
                     wxEmptyString,
                     L"macro.macro.json",
                     TR(IDS_FILTER_MACRO),
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    const auto path = std::filesystem::path(dlg.GetPath().ToStdWstring());

    nlohmann::json j;
    j["version"] = kMacroVersion;
    j["screen"] = utils::GetPrimaryScreenSpec();
    j["run"] = collectRunSpec();
    j["actions"] = m_actions;

    if (!jsonio::saveTo(path, j)) {
        wxMessageBox(TR(IDS_MSG_EXPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
        return;
    }

    wxMessageBox(TR(IDS_MSG_EXPORT_SUCCESS), TR(IDS_APP_TITLE),
                 wxOK | wxICON_INFORMATION, this);
    LOG_INFO("recorder: exported {} actions to {}",
             m_actions.size(), utils::ToUtf8(path.wstring()));
}

// ============================================================
// 坐标适配询问
// ============================================================
RecorderPanel::AdaptChoice
RecorderPanel::askAdaptCoordinates(const ScreenSpec &scriptSpec,
                                   const ScreenSpec &currentSpec) {
    auto &cfgMgr = ConfigManager::instance();

    // 记忆命中：不弹窗
    const auto pref = cfgMgr.config().flags.adaptationPreference;
    if (pref == AdaptationPreference::AlwaysAdapt) {
        LOG_INFO("recorder: adapt preference = AlwaysAdapt, skip prompt");
        return AdaptChoice::Adapt;
    }
    if (pref == AdaptationPreference::KeepOriginal) {
        LOG_INFO("recorder: adapt preference = KeepOriginal, skip prompt");
        return AdaptChoice::KeepOriginal;
    }

    // Ask：弹窗
    const wxString msg = wxString::Format(
        TR(IDS_MSG_ADAPT_CONFIRM).data(),
        scriptSpec.width, scriptSpec.height, scriptSpec.dpi,
        currentSpec.width, currentSpec.height, currentSpec.dpi);

    wxRichMessageDialog dlg(this, msg, TR(IDS_APP_TITLE),
                            wxYES_NO | wxCANCEL | wxICON_QUESTION);
    dlg.SetYesNoLabels(TR(IDS_MSG_ADAPT_YES).data(),
                       TR(IDS_MSG_ADAPT_NO).data());
    dlg.ShowCheckBox(TR(IDS_MSG_ADAPT_REMEMBER));

    const int ret = dlg.ShowModal();

    if (ret == wxID_CANCEL) {
        LOG_DEBUG("recorder: adapt prompt cancelled");
        return AdaptChoice::Cancelled;
    }

    const bool adapt = (ret == wxID_YES);

    if (dlg.IsCheckBoxChecked()) {
        const auto chosen = adapt ? AdaptationPreference::AlwaysAdapt
                                  : AdaptationPreference::KeepOriginal;
        cfgMgr.setAdaptationPreference(chosen);
        LOG_INFO("recorder: adapt preference remembered as {}",
                 adaptationPreferenceToString(chosen));
    }

    return adapt ? AdaptChoice::Adapt : AdaptChoice::KeepOriginal;
}
