#include "ui/panels/RecorderPanel.hpp"

#include <wx/spinctrl.h>
#include <wx/statbox.h>

#include "common/Log.hpp"
#include "managers/LanguageManager.hpp"

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
                                    wxDefaultPosition, wxSize(-1, 200),
                                    wxLC_REPORT | wxLC_SINGLE_SEL);
        m_listCtrl->AppendColumn(TR(IDS_RECORDER_LIST_COL_INDEX), wxLIST_FORMAT_RIGHT, 40);
        m_listCtrl->AppendColumn(TR(IDS_RECORDER_LIST_COL_ACTION), wxLIST_FORMAT_LEFT, 100);
        m_listCtrl->AppendColumn(TR(IDS_RECORDER_LIST_COL_PARAM), wxLIST_FORMAT_LEFT, 140);
        m_listCtrl->AppendColumn(TR(IDS_RECORDER_LIST_COL_DELAY), wxLIST_FORMAT_RIGHT, 80);
        m_listCtrl->AppendColumn(TR(IDS_RECORDER_LIST_COL_HOLD), wxLIST_FORMAT_RIGHT, 80);

        // ---- 临时假数据：仅用于预览列宽，接逻辑时删除 ----
        const auto addRow = [this](const long idx, const wxString &action,
                                   const wxString &param, const wxString &delay,
                                   const wxString &hold) {
            m_listCtrl->InsertItem(idx, wxString::Format("%ld", idx + 1));
            m_listCtrl->SetItem(idx, 1, action);
            m_listCtrl->SetItem(idx, 2, param);
            m_listCtrl->SetItem(idx, 3, delay);
            m_listCtrl->SetItem(idx, 4, hold);
        };
        addRow(0, "Mouse Move", "(960, 540)", "120", "-");
        addRow(1, "Left Click", "(960, 540)", "0", "50");
        addRow(2, "Wheel Up", "3 steps", "50", "-");
        // ---- 假数据结束 ----

        box->Add(m_listCtrl, 0, wxEXPAND | wxALL, 8);

        auto *btnRow = new wxBoxSizer(wxHORIZONTAL);
        m_btnEdit = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_EDIT));
        m_btnDelete = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_DELETE));
        m_btnMoveUp = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_MOVE_UP));
        m_btnMoveDown = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_MOVE_DOWN));
        m_btnClear = new wxButton(staticBox, wxID_ANY, TR(IDS_RECORDER_BTN_CLEAR));

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

        auto *form = new wxFlexGridSizer(2, wxSize(8, 8));
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

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_COMMON_LABEL_RUNMODE)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_choiceRunMode, 1, wxEXPAND);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_COMMON_LABEL_DURATION)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_spinDuration, 1, wxEXPAND);

        form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_COMMON_LABEL_COUNT)),
                  0, wxALIGN_CENTER_VERTICAL);
        form->Add(m_spinCount, 1, wxEXPAND);

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
    m_btnEdit->Bind(wxEVT_BUTTON, &RecorderPanel::onEdit, this);
    m_btnDelete->Bind(wxEVT_BUTTON, &RecorderPanel::onDelete, this);
    m_btnMoveUp->Bind(wxEVT_BUTTON, &RecorderPanel::onMoveUp, this);
    m_btnMoveDown->Bind(wxEVT_BUTTON, &RecorderPanel::onMoveDown, this);
    m_btnClear->Bind(wxEVT_BUTTON, &RecorderPanel::onClear, this);
    m_btnStartPlayback->Bind(wxEVT_BUTTON, &RecorderPanel::onStartPlayback, this);
    m_btnStopPlayback->Bind(wxEVT_BUTTON, &RecorderPanel::onStopPlayback, this);
    m_btnImportScript->Bind(wxEVT_BUTTON, &RecorderPanel::onImportScript, this);
    m_btnExportScript->Bind(wxEVT_BUTTON, &RecorderPanel::onExportScript, this);
}

// ============================================================
// 事件处理（纯 UI 阶段：只记录日志，不接实际逻辑）
// ============================================================

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onStartRecord(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: start recording clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onStopRecord(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: stop recording clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onEdit(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: edit clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onDelete(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: delete clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onMoveUp(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: move up clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onMoveDown(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: move down clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onClear(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: clear clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onStartPlayback(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: start playback clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onStopPlayback(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: stop playback clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onImportScript(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: import script clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void RecorderPanel::onExportScript(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("recorder: export script clicked");
}
