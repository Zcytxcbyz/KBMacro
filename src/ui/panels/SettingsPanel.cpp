#include "ui/panels/SettingsPanel.hpp"

#include <filesystem>
#include <iterator>

#include <wx/statbox.h>

#include "common/Log.hpp"
#include "common/Utils.hpp"
#include "managers/LanguageManager.hpp"

namespace {
    // 计时器模式下拉框的索引
    constexpr int kTimerModeSleep = 0;
    constexpr int kTimerModeSpin = 1;

    // 热键动作列表（顺序即列表行序）
    constexpr int kHotkeyActionResIds[] = {
        IDS_SETTINGS_HOTKEY_START_CLICKER,
        IDS_SETTINGS_HOTKEY_STOP_CLICKER,
        IDS_SETTINGS_HOTKEY_START_KEYBOARD,
        IDS_SETTINGS_HOTKEY_STOP_KEYBOARD,
        IDS_SETTINGS_HOTKEY_START_RECORD,
        IDS_SETTINGS_HOTKEY_STOP_RECORD,
        IDS_SETTINGS_HOTKEY_START_PLAYBACK,
        IDS_SETTINGS_HOTKEY_STOP_PLAYBACK,
        IDS_SETTINGS_HOTKEY_START_ALL,
        IDS_SETTINGS_HOTKEY_STOP_ALL,
    };
}

SettingsPanel::SettingsPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY) {
    buildUi();
}

void SettingsPanel::buildUi() {
    auto *root = new wxBoxSizer(wxVERTICAL);

    buildGeneralGroup(root);
    buildHotkeyGroup(root);
    buildConfigGroup(root);
    buildResetGroup(root);

    SetSizer(root);

    // ---- 事件绑定 ----
    m_choiceLanguage->Bind(wxEVT_CHOICE, &SettingsPanel::onLanguageChanged, this);
    m_choiceTimerMode->Bind(wxEVT_CHOICE, &SettingsPanel::onTimerModeChanged, this);
    m_chkAlwaysOnTop->Bind(wxEVT_CHECKBOX, &SettingsPanel::onAlwaysOnTopToggled, this);
    m_btnModifyHotkey->Bind(wxEVT_BUTTON, &SettingsPanel::onModifyHotkey, this);
    m_btnExportConfig->Bind(wxEVT_BUTTON, &SettingsPanel::onExportConfig, this);
    m_btnClearAdaptation->Bind(wxEVT_BUTTON, &SettingsPanel::onClearAdaptation, this);
    m_btnClearSpinConfirm->Bind(wxEVT_BUTTON, &SettingsPanel::onClearSpinConfirm, this);
    m_btnResetDefaults->Bind(wxEVT_BUTTON, &SettingsPanel::onResetDefaults, this);
    m_btnDeletePortableConfig->Bind(wxEVT_BUTTON, &SettingsPanel::onDeletePortableConfig, this);
}

// ============================================================
// 分组一：通用
// ============================================================
void SettingsPanel::buildGeneralGroup(wxSizer *parent) {
    auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_SETTINGS_GROUP_GENERAL));
    auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

    auto *form = new wxFlexGridSizer(2, wxSize(8, 8));
    form->AddGrowableCol(1, 1);

    // 语言
    m_choiceLanguage = new wxChoice(staticBox, wxID_ANY);
    m_choiceLanguage->Append(L"简体中文");
    m_choiceLanguage->Append(L"English");
    m_choiceLanguage->SetSelection(0);

    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_LANGUAGE)),
              0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_choiceLanguage, 1, wxEXPAND);

    // 语言提示（占位空标签 + 提示）
    form->AddSpacer(0);
    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_HINT_RESTART)),
              1, wxEXPAND);

    // 计时器模式
    m_choiceTimerMode = new wxChoice(staticBox, wxID_ANY);
    m_choiceTimerMode->Append(TR(IDS_SETTINGS_TIMER_SLEEP));
    m_choiceTimerMode->Append(TR(IDS_SETTINGS_TIMER_SPIN));
    m_choiceTimerMode->SetSelection(kTimerModeSleep);

    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_TIMER)),
              0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_choiceTimerMode, 1, wxEXPAND);

    box->Add(form, 0, wxEXPAND | wxALL, 8);

    // 窗口置顶
    m_chkAlwaysOnTop = new wxCheckBox(staticBox, wxID_ANY, TR(IDS_SETTINGS_CHK_ONTOP));
    box->Add(m_chkAlwaysOnTop, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    parent->Add(box, 0, wxEXPAND | wxALL, 8);
}

// ============================================================
// 分组二：热键
// ============================================================
void SettingsPanel::buildHotkeyGroup(wxSizer *parent) {
    auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_SETTINGS_GROUP_HOTKEYS));
    auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

    m_hotkeyList = new wxListCtrl(staticBox, wxID_ANY,
                                  wxDefaultPosition, wxSize(-1, 150),
                                  wxLC_REPORT | wxLC_SINGLE_SEL);
    m_hotkeyList->EnableCheckBoxes(true);
    m_hotkeyList->AppendColumn(TR(IDS_SETTINGS_LIST_COL_ACTION), wxLIST_FORMAT_LEFT, 200);
    m_hotkeyList->AppendColumn(TR(IDS_SETTINGS_LIST_COL_HOTKEY), wxLIST_FORMAT_LEFT, 150);

    for (std::size_t i = 0; i < std::size(kHotkeyActionResIds); ++i) {
        const long row = m_hotkeyList->InsertItem(static_cast<long>(i),
                                                  TR(kHotkeyActionResIds[i]));
        m_hotkeyList->SetItem(row, 1, TR(IDS_SETTINGS_HOTKEY_UNSET));
        m_hotkeyList->CheckItem(row, true);
    }

    box->Add(m_hotkeyList, 0, wxEXPAND | wxALL, 8);

    auto *btnRow = new wxBoxSizer(wxHORIZONTAL);
    m_btnModifyHotkey = new wxButton(staticBox, wxID_ANY, TR(IDS_SETTINGS_BTN_MODIFY_HOTKEY));
    btnRow->Add(m_btnModifyHotkey, 0, wxALL, 4);
    btnRow->AddStretchSpacer(1);

    box->Add(btnRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

    parent->Add(box, 0, wxEXPAND | wxALL, 8);
}

// ============================================================
// 分组三：配置管理
// ============================================================
void SettingsPanel::buildConfigGroup(wxSizer *parent) {
    auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_SETTINGS_GROUP_CONFIG));
    auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

    auto *form = new wxFlexGridSizer(2, wxSize(8, 8));
    form->AddGrowableCol(1, 1);

    m_configPathText = new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_CONFIG_PATH_UNIMPL));

    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_CONFIG_PATH)),
              0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_configPathText, 1, wxEXPAND);

    box->Add(form, 0, wxEXPAND | wxALL, 8);

    auto *btnRow = new wxBoxSizer(wxHORIZONTAL);
    m_btnExportConfig = new wxButton(staticBox, wxID_ANY, TR(IDS_SETTINGS_BTN_EXPORT_CONFIG));
    btnRow->Add(m_btnExportConfig, 0, wxALL, 4);
    btnRow->AddStretchSpacer(1);

    box->Add(btnRow, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);

    auto *hint = new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_HINT_EXPORT));
    box->Add(hint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    parent->Add(box, 0, wxEXPAND | wxALL, 8);
}

// ============================================================
// 分组四：重置
// ============================================================
void SettingsPanel::buildResetGroup(wxSizer *parent) {
    auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_SETTINGS_GROUP_RESET));
    auto *box = new wxStaticBoxSizer(staticBox, wxHORIZONTAL);

    m_btnClearAdaptation = new wxButton(staticBox, wxID_ANY, TR(IDS_SETTINGS_BTN_CLEAR_ADAPT));
    m_btnClearSpinConfirm = new wxButton(staticBox, wxID_ANY, TR(IDS_SETTINGS_BTN_CLEAR_SPIN_CONFIRM));
    m_btnResetDefaults = new wxButton(staticBox, wxID_ANY, TR(IDS_SETTINGS_BTN_RESET_DEFAULTS));
    m_btnDeletePortableConfig = new wxButton(staticBox, wxID_ANY, TR(IDS_SETTINGS_BTN_DELETE_PORTABLE));

    box->Add(m_btnClearAdaptation, 0, wxALL, 4);
    box->Add(m_btnClearSpinConfirm, 0, wxALL, 4);
    box->AddStretchSpacer(1);
    box->Add(m_btnResetDefaults, 0, wxALL, 4);
    box->Add(m_btnDeletePortableConfig, 0, wxALL, 4);

    parent->Add(box, 0, wxEXPAND | wxALL, 8);
}

// ============================================================
// 事件处理
// ============================================================

void SettingsPanel::onLanguageChanged(wxCommandEvent &event) {
    LOG_DEBUG("settings: language changed, index={}", event.GetSelection());
    wxMessageBox(TR(IDS_SETTINGS_MSG_RESTART_REQUIRED),
                 TR(IDS_APP_TITLE),
                 wxOK | wxICON_INFORMATION, this);
}

void SettingsPanel::onTimerModeChanged(const wxCommandEvent &event) {
    const int sel = event.GetSelection();
    LOG_DEBUG("settings: timer mode changed, index={}", sel);
    if (sel == kTimerModeSpin) {
        const int ret = wxMessageBox(TR(IDS_SETTINGS_MSG_SPIN_WARNING),
                                     TR(IDS_APP_TITLE),
                                     wxOK | wxCANCEL | wxICON_WARNING, this);
        if (ret != wxOK) {
            m_choiceTimerMode->SetSelection(kTimerModeSleep);
        }
    }
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void SettingsPanel::onAlwaysOnTopToggled(wxCommandEvent &event) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("settings: always on top = {}", event.IsChecked());
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void SettingsPanel::onModifyHotkey(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("settings: modify hotkey clicked");
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void SettingsPanel::onExportConfig(wxCommandEvent &) { // NOLINT(*-convert-member-functions-to-static)
    LOG_DEBUG("settings: export config clicked");
}

void SettingsPanel::onClearAdaptation(wxCommandEvent &) {
    LOG_DEBUG("settings: clear adaptation clicked");
    const int ret = wxMessageBox(TR(IDS_SETTINGS_MSG_CLEAR_ADAPT_CONFIRM),
                                 TR(IDS_APP_TITLE),
                                 wxOK | wxCANCEL | wxICON_QUESTION, this);
    if (ret == wxOK) {
        LOG_DEBUG("settings: clear adaptation confirmed (not implemented)");
    }
}

void SettingsPanel::onClearSpinConfirm(wxCommandEvent &) {
    LOG_DEBUG("settings: clear spin confirmation clicked");
    const int ret = wxMessageBox(TR(IDS_SETTINGS_MSG_CLEAR_SPIN_CONFIRM),
                                 TR(IDS_APP_TITLE),
                                 wxOK | wxCANCEL | wxICON_QUESTION, this);
    if (ret == wxOK) {
        LOG_DEBUG("settings: clear spin confirmation confirmed (not implemented)");
    }
}

void SettingsPanel::onResetDefaults(wxCommandEvent &) {
    LOG_DEBUG("settings: reset defaults clicked");
    const int ret = wxMessageBox(TR(IDS_SETTINGS_MSG_RESET_CONFIRM),
                                 TR(IDS_APP_TITLE),
                                 wxOK | wxCANCEL | wxICON_QUESTION, this);
    if (ret == wxOK) {
        LOG_DEBUG("settings: reset defaults confirmed (not implemented)");
    }
}

void SettingsPanel::onDeletePortableConfig(wxCommandEvent &) {
    LOG_DEBUG("settings: delete portable config clicked");

    const auto exePath = utils::GetExecutablePath();
    const auto portablePath = std::filesystem::path(exePath)
                              .concat(L".json");

    std::error_code ec;
    if (!std::filesystem::exists(portablePath, ec) || ec) {
        wxMessageBox(TR(IDS_SETTINGS_MSG_DELETE_NO_FILE),
                     TR(IDS_APP_TITLE),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    const wxString msg = TR(IDS_SETTINGS_MSG_DELETE_CONFIRM)
                         + L"\n\n" + portablePath.wstring();
    const int ret = wxMessageBox(msg, TR(IDS_APP_TITLE),
                                 wxOK | wxCANCEL | wxICON_QUESTION, this);
    if (ret == wxOK) {
        LOG_DEBUG("settings: delete portable config confirmed (not implemented)");
    }
}
