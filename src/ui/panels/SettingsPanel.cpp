#include "ui/panels/SettingsPanel.hpp"

#include <filesystem>
#include <iterator>

#include <wx/statbox.h>

#include "common/Hotkey.hpp"
#include "common/HotkeyAction.hpp"
#include "common/Log.hpp"
#include "managers/ConfigManager.hpp"
#include "managers/HotkeyManager.hpp"
#include "managers/LanguageManager.hpp"
#include "ui/dialogs/HotkeyCaptureDialog.hpp"

namespace {
    // 计时器模式下拉框的索引，与 kTimerModeEntries 顺序一致
    constexpr int kTimerModeSleep = 0;
    constexpr int kTimerModeSpin  = 1;
}

SettingsPanel::SettingsPanel(wxWindow *parent, HotkeyManager &hotkeyMgr)
    : wxPanel(parent, wxID_ANY),
      m_hotkeyMgr(hotkeyMgr) {
    buildUi();
    refreshFromConfig();
    refreshHotkeyList();
    refreshConfigPathText();
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

    // 语言：从 kLangs 遍历，索引 ↔ 语言一一对应
    m_choiceLanguage = new wxChoice(staticBox, wxID_ANY);
    for (const auto &lang: kLangs) {
        m_choiceLanguage->Append(wxString(lang.displayName.data(), lang.displayName.size()));
    }

    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_LANGUAGE)),
              0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_choiceLanguage, 1, wxEXPAND);

    form->AddSpacer(0);
    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_HINT_RESTART)),
              1, wxEXPAND);

    // 计时器模式
    m_choiceTimerMode = new wxChoice(staticBox, wxID_ANY);
    m_choiceTimerMode->Append(TR(IDS_SETTINGS_TIMER_SLEEP));
    m_choiceTimerMode->Append(TR(IDS_SETTINGS_TIMER_SPIN));

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

    // 列表内容由 refreshHotkeyList() 填，构造末尾会调一次

    box->Add(m_hotkeyList, 0, wxEXPAND | wxALL, 8);

    auto *btnRow = new wxBoxSizer(wxHORIZONTAL);
    m_btnModifyHotkey = new wxButton(staticBox, wxID_ANY, TR(IDS_SETTINGS_BTN_MODIFY_HOTKEY));
    btnRow->Add(m_btnModifyHotkey, 0, wxALL, 4);
    btnRow->AddStretchSpacer(1);

    box->Add(btnRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

    parent->Add(box, 0, wxEXPAND | wxALL, 8);

    m_hotkeyList->Bind(wxEVT_LIST_ITEM_SELECTED,
                       &SettingsPanel::onHotkeySelectionChanged, this);
    m_hotkeyList->Bind(wxEVT_LIST_ITEM_DESELECTED,
                       &SettingsPanel::onHotkeySelectionChanged, this);

    // 初始无选中 → 按钮禁用
    m_btnModifyHotkey->Enable(false);
}

// ============================================================
// 分组三：配置管理
// ============================================================
void SettingsPanel::buildConfigGroup(wxSizer *parent) {
    auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_SETTINGS_GROUP_CONFIG));
    auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

    auto *form = new wxFlexGridSizer(2, wxSize(8, 8));
    form->AddGrowableCol(1, 1);

    m_configPathText = new wxStaticText(staticBox, wxID_ANY, wxEmptyString);
    m_configPathText->SetMinSize(wxSize(280, -1));

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
// 从 ConfigManager 同步到控件
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::refreshFromConfig() {
    const auto &cfg = ConfigManager::instance().config();

    // 语言：按 LanguageManager 当前状态定位
    const Lang cur = LanguageManager::instance().currentLang();
    for (std::size_t i = 0; i < std::size(kLangs); ++i) {
        if (kLangs[i].id == cur) {
            m_choiceLanguage->SetSelection(static_cast<int>(i));
            break;
        }
    }

    // 计时器模式
    m_choiceTimerMode->SetSelection(
        cfg.timerMode == TimerMode::Spin ? kTimerModeSpin : kTimerModeSleep);

    // 窗口置顶
    m_chkAlwaysOnTop->SetValue(cfg.alwaysOnTop);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::refreshHotkeyList() {
    m_hotkeyList->DeleteAllItems();

    const auto &hotkeys = ConfigManager::instance().config().hotkeys;
    for (std::size_t i = 0; i < std::size(kHotkeyActionResIds); ++i) {
        const long row = m_hotkeyList->InsertItem(static_cast<long>(i),
                                                  TR(kHotkeyActionResIds[i]));
        const std::wstring s = utils::hotkeyToString(hotkeys[i]);
        m_hotkeyList->SetItem(row, 1,
                              s.empty() ? TR(IDS_SETTINGS_HOTKEY_UNSET)
                                        : wxString(s));
        m_hotkeyList->CheckItem(row, true);
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::refreshConfigPathText() {
    const auto &path = ConfigManager::instance().currentPath();
    m_configPathText->SetLabel(wxString(path.wstring()));
}

// ============================================================
// 事件处理
// ============================================================

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void SettingsPanel::onLanguageChanged(wxCommandEvent &event) {
    const int sel = event.GetSelection();
    if (sel < 0 || sel >= static_cast<int>(std::size(kLangs))) return;

    LOG_DEBUG("settings: language changed to '{}'", kLangs[sel].code);
    ConfigManager::instance().setLanguage(kLangs[sel].code);

    wxMessageBox(TR(IDS_SETTINGS_MSG_RESTART_REQUIRED),
                 TR(IDS_APP_TITLE),
                 wxOK | wxICON_INFORMATION, this);
}

void SettingsPanel::onTimerModeChanged(const wxCommandEvent &event) {
    const int sel = event.GetSelection();
    if (sel < 0) return;

    LOG_DEBUG("settings: timer mode changed, index={}", sel);

    const TimerMode newMode = (sel == kTimerModeSpin)
                                  ? TimerMode::Spin
                                  : TimerMode::Sleep;
    auto &cfgMgr = ConfigManager::instance();

    if (newMode == TimerMode::Spin && !cfgMgr.config().flags.spinWarningConfirmed) {
        const int ret = wxMessageBox(TR(IDS_SETTINGS_MSG_SPIN_WARNING),
                                     TR(IDS_APP_TITLE),
                                     wxOK | wxCANCEL | wxICON_WARNING, this);
        if (ret != wxOK) {
            // 用户取消：控件回退，配置不动
            m_choiceTimerMode->SetSelection(kTimerModeSleep);
            return;
        }
        // 记住已确认，下次不再提醒（除非"清除高精度确认"）
        cfgMgr.setSpinWarningConfirmed(true);
    }

    cfgMgr.setTimerMode(newMode);
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void SettingsPanel::onAlwaysOnTopToggled(wxCommandEvent &event) {
    const bool on = event.IsChecked();
    LOG_DEBUG("settings: always on top = {}", on);

    // 落盘
    ConfigManager::instance().setAlwaysOnTop(on);

    // 立即作用于窗口
    auto *frame = wxDynamicCast(wxGetTopLevelParent(this), wxFrame);
    if (!frame) {
        LOG_WARN("settings: top-level frame not found");
        return;
    }

    int style = frame->GetWindowStyleFlag();
    if (on) {
        style |= wxSTAY_ON_TOP;
    } else {
        style &= ~wxSTAY_ON_TOP;
    }
    frame->SetWindowStyleFlag(style);

    // MSW 下 SetWindowStyleFlag 对 wxSTAY_ON_TOP 的切换不会自动重排 z-order，
    // 手动 Raise 一次让窗口状态立即反映
    frame->Raise();
}

void SettingsPanel::onModifyHotkey(wxCommandEvent &) {
    const long row = m_hotkeyList->GetNextItem(-1, wxLIST_NEXT_ALL,
                                               wxLIST_STATE_SELECTED);
    if (row < 0) {
        LOG_DEBUG("settings: modify hotkey clicked, but no row selected");
        return;
    }

    const auto index = static_cast<int>(row);
    const Hotkey current = ConfigManager::instance().config().hotkeys[index];

    // 冲突检测：闭包捕获 index，排除自身
    HotkeyCaptureDialog dlg(
        this,
        TR(IDS_SETTINGS_BTN_MODIFY_HOTKEY),
        current,
        /*requireMainKey=*/true,
        [this, index](const Hotkey &hk) {
            return m_hotkeyMgr.isConflicting(hk, index);
        });

    if (dlg.ShowModal() != wxID_OK) return;

    // setHotkey 内部：冲突检查 → 注销旧 → 落盘 → 注册新
    // 冲突时会弹提示（HotkeyCaptureDialog 已先行检查），此处失败一般是"被其他程序占用"
    if (!m_hotkeyMgr.setHotkey(index, dlg.getHotkey())) {
        LOG_WARN("settings: hotkey[{}] set failed (occupied?)", index);
    }

    refreshHotkeyList();
}

void SettingsPanel::onExportConfig(wxCommandEvent &) {
    LOG_DEBUG("settings: export config clicked");

    ConfigManager::instance().exportToPortable();

    // 导出成功后路径已切到便携文件，刷新显示
    refreshConfigPathText();
}

void SettingsPanel::onClearAdaptation(wxCommandEvent &) {
    LOG_DEBUG("settings: clear adaptation clicked");

    const int ret = wxMessageBox(TR(IDS_SETTINGS_MSG_CLEAR_ADAPT_CONFIRM),
                                 TR(IDS_APP_TITLE),
                                 wxOK | wxCANCEL | wxICON_QUESTION, this);
    if (ret != wxOK) return;

    // TODO: Recorder 模块落地后接适配记忆的清理
    LOG_DEBUG("settings: clear adaptation confirmed (recorder not implemented)");
}

void SettingsPanel::onClearSpinConfirm(wxCommandEvent &) {
    LOG_DEBUG("settings: clear spin confirmation clicked");

    const int ret = wxMessageBox(TR(IDS_SETTINGS_MSG_CLEAR_SPIN_CONFIRM),
                                 TR(IDS_APP_TITLE),
                                 wxOK | wxCANCEL | wxICON_QUESTION, this);
    if (ret != wxOK) return;

    ConfigManager::instance().clearSpinWarning();
}

void SettingsPanel::onResetDefaults(wxCommandEvent &) {
    LOG_DEBUG("settings: reset defaults clicked");

    const int ret = wxMessageBox(TR(IDS_SETTINGS_MSG_RESET_CONFIRM),
                                 TR(IDS_APP_TITLE),
                                 wxOK | wxCANCEL | wxICON_QUESTION, this);
    if (ret != wxOK) return;

    ConfigManager::instance().resetToDefaults();

    // 重置后：配置里的热键都变空，需要重新注册
    // HotkeyManager::registerAll 会遍历 config，逐个注册（空热键跳过）
    m_hotkeyMgr.registerAll();

    // 刷新所有控件回默认状态
    refreshFromConfig();
    refreshHotkeyList();
    refreshConfigPathText();

    // 窗口置顶也要回默认（false）
    if (auto *frame = wxDynamicCast(wxGetTopLevelParent(this), wxFrame)) {
        int style = frame->GetWindowStyleFlag();
        style &= ~wxSTAY_ON_TOP;
        frame->SetWindowStyleFlag(style);
        frame->Raise();
    }

    LOG_INFO("settings: reset to defaults completed");
}

void SettingsPanel::onDeletePortableConfig(wxCommandEvent &) {
    LOG_DEBUG("settings: delete portable config clicked");

    if (!ConfigManager::instance().isPortable()) {
        wxMessageBox(TR(IDS_SETTINGS_MSG_DELETE_NO_FILE),
                     TR(IDS_APP_TITLE),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    const wxString msg = TR(IDS_SETTINGS_MSG_DELETE_CONFIRM)
                         + L"\n\n" + ConfigManager::instance().currentPath().wstring();
    const int ret = wxMessageBox(msg, TR(IDS_APP_TITLE),
                                 wxOK | wxCANCEL | wxICON_QUESTION, this);
    if (ret != wxOK) return;

    ConfigManager::instance().deletePortable();
    refreshConfigPathText();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::onHotkeySelectionChanged(wxListEvent &event) {
    const long row = m_hotkeyList->GetNextItem(-1, wxLIST_NEXT_ALL,
                                               wxLIST_STATE_SELECTED);
    m_btnModifyHotkey->Enable(row >= 0);
    event.Skip();
}
