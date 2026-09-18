#include "ui/panels/ClickerPanel.hpp"

#include <wx/spinctrl.h>
#include <wx/statbox.h>

#include "common/Log.hpp"
#include "managers/LanguageManager.hpp"

ClickerPanel::ClickerPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
    buildUi();
}

void ClickerPanel::buildUi() {
    auto *root = new wxBoxSizer(wxVERTICAL);

    // ---- 分组一：坐标 ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_CLICKER_GROUP_POS));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        auto *form = new wxFlexGridSizer(2, wxSize(8, 8));
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

        auto *form = new wxFlexGridSizer(2, wxSize(8, 8));
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
    m_btnStart->Bind(wxEVT_BUTTON, &ClickerPanel::onStart, this);
    m_btnStop->Bind(wxEVT_BUTTON, &ClickerPanel::onStop, this);
    m_btnImport->Bind(wxEVT_BUTTON, &ClickerPanel::onImport, this);
    m_btnExport->Bind(wxEVT_BUTTON, &ClickerPanel::onExport, this);
}

// ============================================================
// 事件处理（纯 UI 阶段：只记录日志，不接实际逻辑）
// ============================================================

// ReSharper disable once CppMemberFunctionMayBeStatic
void ClickerPanel::onCapturePos(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("clicker: capture position clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void ClickerPanel::onIgnorePos(wxCommandEvent &event) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("clicker: ignore position = {}", event.IsChecked());
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void ClickerPanel::onStart(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("clicker: start clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void ClickerPanel::onStop(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("clicker: stop clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void ClickerPanel::onImport(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("clicker: import clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void ClickerPanel::onExport(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("clicker: export clicked");
}
