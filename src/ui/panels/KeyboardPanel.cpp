#include "ui/panels/KeyboardPanel.hpp"

#include <wx/spinctrl.h>
#include <wx/statbox.h>

#include "common/Log.hpp"
#include "managers/LanguageManager.hpp"

KeyboardPanel::KeyboardPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
    buildUi();
}

void KeyboardPanel::buildUi() {
    auto *root = new wxBoxSizer(wxVERTICAL);

    // ---- 分组一：按键 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_KEYBOARD_GROUP_KEY));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        auto *form = new wxFlexGridSizer(2, wxSize(8, 8));
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

        auto *form = new wxFlexGridSizer(2, wxSize(8, 8));
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
}

// ============================================================
// 事件处理（纯 UI 阶段：只记录日志，不接实际逻辑）
// ============================================================

// ReSharper disable once CppMemberFunctionMayBeStatic
void KeyboardPanel::onSetKey(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("keyboard: set key clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void KeyboardPanel::onSendOnce(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("keyboard: send once clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void KeyboardPanel::onStart(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("keyboard: start clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void KeyboardPanel::onStop(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("keyboard: stop clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void KeyboardPanel::onImport(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("keyboard: import clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void KeyboardPanel::onExport(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("keyboard: export clicked");
}
