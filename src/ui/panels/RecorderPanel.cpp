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
#include "managers/ThreadManager.hpp"
#include "ui/dialogs/ActionEditDialog.hpp"
#include "ui/DialogButtons.hpp"

namespace {

    // ---- Action → 显示文本 ----

    wxString actionTypeText(const ActionType t) {
        switch (t) {
            case ActionType::MouseMove:  return {TR(IDS_ACTION_TYPE_MOUSE_MOVE)};
            case ActionType::MouseClick: return {TR(IDS_ACTION_TYPE_MOUSE_CLICK)};
            case ActionType::MouseWheel: return {TR(IDS_ACTION_TYPE_MOUSE_WHEEL)};
            case ActionType::Keyboard:   return {TR(IDS_ACTION_TYPE_KEYBOARD)};
            case ActionType::MouseDown:  return {TR(IDS_ACTION_TYPE_MOUSE_DOWN)};
            case ActionType::MouseUp:    return {TR(IDS_ACTION_TYPE_MOUSE_UP)};
            case ActionType::Delay:      return {TR(IDS_ACTION_TYPE_DELAY)};
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

    // 相对动作在参数列末尾标一下，一眼能区分绝对/相对
    wxString relativeSuffix(const Action &a) {
        return a.relative ? wxString(" ") + wxString(TR(IDS_ACTION_RELATIVE_SUFFIX)) : wxString{};
    }

    wxString actionParamText(const Action &a) {
        switch (a.type) {
            case ActionType::MouseMove:
                return wxString::Format("(%d, %d)", a.x, a.y) + relativeSuffix(a);
            case ActionType::MouseClick:
                return wxString::Format("%s (%d, %d)", mouseButtonText(a.button), a.x, a.y) +
                       relativeSuffix(a);
            case ActionType::MouseWheel:
                return wxString::Format("%s x%d", wheelDirText(a.wheelDirection), a.wheelSteps);
            case ActionType::Keyboard: {
                const auto key = wxString(utils::hotkeyToString(a.hotkey));
                const auto evt = (a.keyEvent == KeyEventType::Down)
                                     ? wxString(TR(IDS_ACTION_KEY_DOWN))
                                     : wxString(TR(IDS_ACTION_KEY_UP));
                return wxString::Format("%s [%s]", key, evt);
            }
            case ActionType::MouseDown:
            case ActionType::MouseUp:
                // 类型列已经写明"按下/松开"，这里只补按键和坐标
                return wxString::Format("%s (%d, %d)", mouseButtonText(a.button), a.x, a.y) +
                       relativeSuffix(a);
            case ActionType::Delay:
                return {};
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

RecorderPanel::RecorderPanel(wxWindow *parent, ThreadManager &threadMgr)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxHSCROLL),
      m_threadMgr(threadMgr) {
    buildUi();
    setRunning(false);
    setRecording(false);

    // 页面双向滚动：横向管"窗口太窄、控件排不下"，竖向管"窗口太矮"。
    // 动作列表自己还有一套滚动条（行数可变、列宽固定），那是列表自己的范围。
    SetScrollRate(FromDIP(10), FromDIP(10));
    wxWindowBase::FitInside();
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

    const bool editable = !m_recording;
    const bool hasSelection = (row >= 0);
    const bool hasItems = (total > 0);

    m_btnEdit->Enable(editable && hasSelection);
    m_btnDelete->Enable(editable && hasSelection);
    m_btnMoveUp->Enable(editable && hasSelection && row > 0);
    m_btnMoveDown->Enable(editable && hasSelection && row < total - 1);
    m_btnClear->Enable(editable && hasItems);
    // 列表变化后同步回放按钮（空列表不允许开始、录制中不允许开始）
    setRunning(m_threadMgr.IsRunning(TaskKind::Playback));
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

void RecorderPanel::startRecording() {
    const auto &cfg = ConfigManager::instance().config();

    recorder::RecordOptions opts;
    opts.startDelayMs   = cfg.recordStartDelayMs;
    opts.moveIntervalMs = cfg.recordMoveIntervalMs;
    opts.ignoreInjected = cfg.recordIgnoreInjected;
    opts.maxActions     = cfg.recordMaxActions;

    // 所有已启用的全局热键主键都过滤：录制期间它们已被热键系统消费、前台程序
    // 收不到，录进产物只会在回放时反过来触发我们自己。
    //   修饰键不在这里传（那靠归一化阶段的头尾剔除），
    //   否则用户正常按的 Ctrl+C 会被吃掉 Ctrl。
    for (int i = 0; i < kHotkeyCount; ++i) {
        if (cfg.hotkeyEnabled[i] && cfg.hotkeys[i].vk != 0) {
            opts.ignoredKeys.push_back(cfg.hotkeys[i].vk);
        }
    }

    // 开始/停止热键的主键与修饰键：只给归一化阶段用于剔除产物头尾的修饰键段
    const auto &startKey = cfg.hotkeys[static_cast<int>(HotkeyAction::StartRecord)];
    const auto &stopKey  = cfg.hotkeys[static_cast<int>(HotkeyAction::StopRecord)];
    opts.startKeyVk = startKey.vk;
    opts.startMods  = startKey.modifiers;
    opts.stopKeyVk  = stopKey.vk;
    opts.stopMods   = stopKey.modifiers;

    LOG_INFO("recorder: start recording requested (delay={}ms, moveInterval={}ms, "
             "ignoreInjected={}, maxActions={}, ignoredKeys={})",
             opts.startDelayMs, opts.moveIntervalMs, opts.ignoreInjected, opts.maxActions,
             opts.ignoredKeys.size());

    m_threadMgr.StartRecord(opts, [this](const TaskResult& res) { onRecordFinished(res); });

    // 回放正在跑时 ThreadManager 会拒绝这次请求（不回调、不打按钮），
    // 所以按权威状态校正，免得界面切到"录制中"却什么都没发生
    setRecording(m_threadMgr.IsRunning(TaskKind::Record));
}

void RecorderPanel::stopRecording() {
    LOG_INFO("recorder: stop recording requested");

    // 阻塞 join，返回时采集线程确实已经退出（钩子也卸了）。
    //   产物走队列里的回调：稍后 onRecordFinished 再问"替换/追加/丢弃"
    m_threadMgr.StopRecord();
    setRecording(false);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void RecorderPanel::startPlayback() {
    const RunSpec run = collectRunSpec();
    LOG_INFO("recorder: start playback requested (actions={}, mode={})",
             m_actions.size(), runModeToString(run.mode));

    // 已在跑时 ThreadManager 会忽略这次请求（同一任务不允许并发），
    // 所以不看返回值，直接按"已启动"切按钮；真结束时回调会再切回来
    m_threadMgr.StartPlayback(m_actions, run, [this](const TaskResult& res) {
        setRunning(false);
        LOG_INFO("recorder: playback finished, reason={}, executed={}", // NOLINT(*-lambda-function-name)
                 static_cast<int>(res.reason), res.executed);
    });
    setRunning(true);
}

void RecorderPanel::stopPlayback() {
    LOG_INFO("recorder: stop playback requested");
    m_threadMgr.StopPlayback();
    setRunning(false);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void RecorderPanel::setRunning(const bool running) {
    // 空列表时不允许开始：否则会起一个"一条动作都不播"的线程
    // 录制中也不允许：录制与回放互斥，ThreadManager 会拒绝，界面就别给入口
    m_btnStartPlayback->Enable(!running && !m_recording && !m_actions.empty());
    m_btnStopPlayback->Enable(running);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void RecorderPanel::setRecording(const bool recording) {
    m_recording = recording;

    m_btnStartRecord->Enable(!recording);
    m_btnStopRecord->Enable(recording);

    // 录制期间锁住"会改动列表"的入口：停止后要拿录到的内容和当前列表做交付选择，
    //   中途改了列表，那个"替换还是追加"的提问就失去意义了
    m_btnAdd->Enable(!recording);
    m_btnImportScript->Enable(!recording);
    m_btnExportScript->Enable(!recording);

    // 编辑类按钮按"选中 + 非录制中"统一刷新
    refreshButtonStates();
}

void RecorderPanel::onRecordFinished(const TaskResult &res) {
    setRecording(false);

    LOG_INFO("recorder: recording finished, reason={}, actions={}", // NOLINT(*-lambda-function-name)
             static_cast<int>(res.reason),
             res.recorded ? static_cast<int>(res.recorded->size()) : -1);

    // 没有产物只有一种情况：录制线程异常结束（钩子装不上等），launch 已经记了 ERROR
    if (!res.recorded) {
        if (res.reason == FinishReason::Failed) {
            wxMessageBox(TR(IDS_MSG_RECORD_FAILED), TR(IDS_APP_TITLE),
                         wxOK | wxICON_ERROR, this);
        }
        return;
    }

    if (res.recorded->empty()) {
        wxMessageBox(TR(IDS_MSG_RECORD_EMPTY), TR(IDS_APP_TITLE),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    deliverRecording(*res.recorded);
}

void RecorderPanel::deliverRecording(const std::vector<Action> &actions) {
    const auto count = static_cast<int>(actions.size());

    // 列表本来就是空的：直接装进去，没必要问"替换还是追加"
    if (m_actions.empty()) {
        m_actions = actions;
        refreshList();
        selectRow(static_cast<long>(m_actions.size()) - 1);
        LOG_INFO("recorder: {} action(s) delivered (list was empty)", count);
        return;
    }

    wxMessageDialog dlg(this,
                        wxString::Format(wxString(TR(IDS_MSG_RECORD_DELIVER)), count),
                        TR(IDS_APP_TITLE),
                        wxYES_NO | wxCANCEL | wxICON_QUESTION);

    dlg.SetYesNoCancelLabels(wxString(TR(IDS_MSG_RECORD_REPLACE)),
                         wxString(TR(IDS_MSG_RECORD_APPEND)),
                         wxString(TR(IDS_MSG_RECORD_CANCEL)));

    switch (dlg.ShowModal()) {
        case wxID_YES:
            m_actions = actions;
            LOG_INFO("recorder: {} action(s) delivered (replaced)", count);
            break;
        case wxID_NO:
            m_actions.insert(m_actions.end(), actions.begin(), actions.end());
            LOG_INFO("recorder: {} action(s) delivered (appended)", count);
            break;
        default:
            LOG_INFO("recorder: {} recorded action(s) discarded by user", count);
            return;
    }

    refreshList();
    selectRow(static_cast<long>(m_actions.size()) - 1);
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
