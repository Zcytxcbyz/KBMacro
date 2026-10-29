#include "ui/panels/SettingsPanel.hpp"

#include <filesystem>
#include <iterator>

#include <wx/statbox.h>
#include <wx/msgdlg.h>
#include <wx/stdpaths.h>
#include <wx/richmsgdlg.h>
#include <wx/spinctrl.h>
#include <wx/utils.h>

#include "common/Hotkey.hpp"
#include "common/HotkeyAction.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"
#include "managers/ConfigManager.hpp"
#include "managers/HotkeyManager.hpp"
#include "managers/LanguageManager.hpp"
#include "ui/dialogs/HotkeyCaptureDialog.hpp"
#include "ui/MainFrame.hpp"

namespace {
    // 计时器模式下拉框的索引，与 kTimerModeEntries 顺序一致
    constexpr int kTimerModeSleep  = 0;
    constexpr int kTimerModeSpin   = 1;
    constexpr int kTimerModeHybrid = 2;

    [[nodiscard]] int timerModeToIndex(const TimerMode mode) {
        switch (mode) {
            case TimerMode::Hybrid: return kTimerModeHybrid;
            case TimerMode::Spin: return kTimerModeSpin;
            case TimerMode::Sleep:
            default: return kTimerModeSleep;
        }
    }

    [[nodiscard]] TimerMode indexToTimerMode(const int index) {
        switch (index) {
            case kTimerModeHybrid: return TimerMode::Hybrid;
            case kTimerModeSpin: return TimerMode::Spin;
            default: return TimerMode::Sleep;
        }
    }
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
    // wxMSW 的 wxSpinCtrl：按上下箭头发 wxEVT_SPINCTRL，手输数字发 wxEVT_TEXT
    // （手输越界值两者都不发，靠焦点离开时的夹取兜底）。两个都接。
    m_spinHybridMs->Bind(wxEVT_SPINCTRL, &SettingsPanel::onHybridSpinChanged, this);
    m_spinHybridMs->Bind(wxEVT_TEXT, &SettingsPanel::onHybridSpinChanged, this);
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

    auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
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
    m_choiceTimerMode->Append(TR(IDS_SETTINGS_TIMER_HYBRID));

    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_TIMER)),
              0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_choiceTimerMode, 1, wxEXPAND);

    // 混合模式：末尾忙等时长（仅 Hybrid 时可编辑，见 refreshFromConfig）
    m_spinHybridMs = new wxSpinCtrl(staticBox, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, FromDIP(wxSize(80, -1)),
                                    wxSP_ARROW_KEYS,
                                    kTimerHybridSpinMsMin, kTimerHybridSpinMsMax,
                                    kTimerHybridSpinMsDefault);
    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_HYBRID_SPIN)),
              0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_spinHybridMs, 0, wxALIGN_CENTER_VERTICAL);

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
                                  wxDefaultPosition, FromDIP(wxSize(-1, 150)),
                                  wxLC_REPORT | wxLC_SINGLE_SEL);
    m_hotkeyList->EnableCheckBoxes(true);
    m_hotkeyList->AppendColumn(TR(IDS_SETTINGS_LIST_COL_ACTION), wxLIST_FORMAT_LEFT, FromDIP(200));
    m_hotkeyList->AppendColumn(TR(IDS_SETTINGS_LIST_COL_HOTKEY), wxLIST_FORMAT_LEFT, FromDIP(150));

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
    m_hotkeyList->Bind(wxEVT_LIST_ITEM_CHECKED,
                       &SettingsPanel::onHotkeyChecked, this);      // ← 新增
    m_hotkeyList->Bind(wxEVT_LIST_ITEM_UNCHECKED,
                       &SettingsPanel::onHotkeyChecked, this);      // ← 新增

    // 初始无选中 → 按钮禁用
    m_btnModifyHotkey->Enable(false);
}

// ============================================================
// 分组三：配置管理
// ============================================================
void SettingsPanel::buildConfigGroup(wxSizer *parent) {
    auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_SETTINGS_GROUP_CONFIG));
    auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

    auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
    form->AddGrowableCol(1, 1);

    m_configPathText = new wxStaticText(staticBox, wxID_ANY, wxEmptyString);
    m_configPathText->SetMinSize(wxSize(FromDIP(280), -1));

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
    m_choiceTimerMode->SetSelection(timerModeToIndex(cfg.timerMode));

    // 末尾忙等时长：只有 Hybrid 模式下才有意义
    m_spinHybridMs->SetValue(cfg.timerHybridSpinMs);
    m_spinHybridMs->Enable(cfg.timerMode == TimerMode::Hybrid);

    // 窗口置顶
    m_chkAlwaysOnTop->SetValue(cfg.alwaysOnTop);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::refreshHotkeyList() {
    m_hotkeyList->DeleteAllItems();

    const auto &cfg = ConfigManager::instance().config();
    for (std::size_t i = 0; i < std::size(kHotkeyActionResIds); ++i) {
        const long row = m_hotkeyList->InsertItem(static_cast<long>(i),
                                                  TR(kHotkeyActionResIds[i]));
        const std::wstring s = utils::hotkeyToString(cfg.hotkeys[i]);
        m_hotkeyList->SetItem(row, 1,
                              s.empty() ? TR(IDS_SETTINGS_HOTKEY_UNSET)
                                        : wxString(s));
        m_hotkeyList->CheckItem(row, cfg.hotkeyEnabled[i]);
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

    const Lang newLang = kLangs[sel].id;
    const Lang curLang = LanguageManager::instance().currentLang();

    // 用户切回当前语言：什么也不做
    if (newLang == curLang) return;

    // 询问对话框：立即重启 / 稍后重启 / 取消
    wxMessageDialog dlg(this,
                        TR(IDS_SETTINGS_MSG_RESTART_ASK),
                        TR(IDS_APP_TITLE),
                        wxYES_NO | wxCANCEL | wxICON_QUESTION);
    dlg.SetYesNoLabels(TR(IDS_SETTINGS_BTN_RESTART_NOW).data(),
                       TR(IDS_SETTINGS_BTN_RESTART_LATER).data());
    const int ret = dlg.ShowModal();

    if (ret == wxID_CANCEL) {
        // 回退控件到当前语言
        for (std::size_t i = 0; i < std::size(kLangs); ++i) {
            if (kLangs[i].id == curLang) {
                m_choiceLanguage->SetSelection(static_cast<int>(i));
                break;
            }
        }
        LOG_DEBUG("settings: language change cancelled");
        return;
    }

    // 落盘（用户主动选过 = 记住）
    ConfigManager::instance().setLanguage(kLangs[sel].code);
    LOG_INFO("settings: language changed to '{}'", kLangs[sel].code);

    if (ret == wxID_YES) {
        // 立即重启
        const wxString exePath = wxStandardPaths::Get().GetExecutablePath();
        LOG_INFO("settings: restarting immediately, exe={}",
                 utils::ToUtf8(exePath.ToStdWstring()));

        // 接下来走 ExitMainLoop，不会触发 wxEVT_CLOSE_WINDOW，
        // 而新进程紧接着就要读配置。所以主动保存一次窗口状态。
        // 顺序很关键：保存 → 启动新进程 → 退出旧进程。
        if (auto *frame = dynamic_cast<MainFrame *>(wxGetTopLevelParent(this))) {
            frame->saveWindowState();
        }

        if (wxExecute(wxString::Format("\"%s\"", exePath), wxEXEC_ASYNC) > 0) {
            // 新进程已启动。
            // 用 ExitMainLoop 而非 Close(true)：
            //   Close 是"请求关闭窗口"，依赖事件循环处理，
            //   在事件 handler 内调用容易被推迟或不生效（表现为新旧窗口并存）。
            //   ExitMainLoop 直接退出事件循环，保证旧进程一定退出。
            // OnExit 会被正常调用，日志 flush 不丢。
            wxTheApp->ExitMainLoop();
        } else {
            // 新进程启动失败：不退出，让用户继续用当前实例
            // 配置已落盘，用户下次手动启动仍是新语言
            LOG_ERROR("settings: failed to launch new instance");
        }
    }
    // wxID_NO = 稍后重启：什么都不做，配置已落盘
}

void SettingsPanel::onTimerModeChanged(const wxCommandEvent &event) {
    const int sel = event.GetSelection();
    if (sel < 0) return;

    LOG_DEBUG("settings: timer mode changed, index={}", sel);

    const TimerMode newMode = indexToTimerMode(sel);
    auto &cfgMgr = ConfigManager::instance();

    // Spin 与 Hybrid 都会烧 CPU（Hybrid 只烧末尾那一段），共用同一条警告
    if (newMode != TimerMode::Sleep && !cfgMgr.config().flags.spinWarningConfirmed) {
        // 用 wxRichMessageDialog：带复选框，让用户主动选择是否记住
        wxRichMessageDialog dlg(this,
                                TR(IDS_SETTINGS_MSG_SPIN_WARNING),
                                TR(IDS_APP_TITLE),
                                wxOK | wxCANCEL | wxICON_WARNING);
        dlg.ShowCheckBox(TR(IDS_SETTINGS_CHK_SPIN_DONT_ASK));
        const int ret = dlg.ShowModal();

        if (ret != wxID_OK) {
            // 用户取消：控件回退到当前生效的模式，配置不动
            m_choiceTimerMode->SetSelection(timerModeToIndex(cfgMgr.config().timerMode));
            return;
        }

        // 只有勾选了"不再提醒"才记住；否则下次切换还会弹
        if (dlg.IsCheckBoxChecked()) {
            LOG_DEBUG("settings: spin warning marked as acknowledged");
            cfgMgr.setSpinWarningConfirmed(true);
        }
    }

    // 末尾忙等时长只对 Hybrid 有意义
    m_spinHybridMs->Enable(newMode == TimerMode::Hybrid);

    cfgMgr.setTimerMode(newMode);

    // 通知主窗口重建计时器实例：只影响此后启动的任务
    if (auto *frame = dynamic_cast<MainFrame *>(wxGetTopLevelParent(this))) {
        frame->rebuildTimer();
    } else {
        LOG_WARN("settings: top-level frame is not MainFrame, timer not rebuilt");
    }
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
// ReSharper disable once CppMemberFunctionMayBeStatic
// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::onHybridSpinChanged(wxCommandEvent &event) {
    (void)event;   // 两个事件类型不同，统一读控件当前值，不依赖 event.GetInt()

    const int ms = m_spinHybridMs->GetValue();
    LOG_DEBUG("settings: hybrid spin tail = {}ms", ms);
    ConfigManager::instance().setTimerHybridSpinMs(ms);
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

    LOG_DEBUG("settings: modify hotkey[{}], current='{}'",
              index, utils::ToUtf8(utils::hotkeyToString(current)));

    // 冲突检测：闭包捕获 index，排除自身
    HotkeyCaptureDialog dlg(
        this,
        TR(IDS_SETTINGS_BTN_MODIFY_HOTKEY),
        current,
        /*requireMainKey=*/true,
        [this, index](const Hotkey &hk) {
            return m_hotkeyMgr.isConflicting(hk, index);
        });

    if (dlg.ShowModal() != wxID_OK) {
        LOG_DEBUG("settings: hotkey[{}] modify cancelled", index);
        return;
    }

    const Hotkey wanted = dlg.getHotkey();
    const std::string wantedStr = utils::ToUtf8(utils::hotkeyToString(wanted));

    // setHotkey 内部：冲突检查 → 注销旧 → 落盘 → 注册新
    // 冲突时会弹提示（HotkeyCaptureDialog 已先行检查），此处失败一般是"被其他程序占用"
    if (!m_hotkeyMgr.setHotkey(index, wanted)) {
        LOG_WARN("settings: hotkey[{}] set to '{}' failed (occupied?)",
                 index, wantedStr);
    } else {
        LOG_INFO("settings: hotkey[{}] set to '{}'",
                 index, wantedStr);
    }

    refreshHotkeyList();
}

void SettingsPanel::onExportConfig(wxCommandEvent &) {
    LOG_DEBUG("settings: export config clicked");

    if (ConfigManager::instance().exportToPortable()) {
        // 成功：路径已切到便携文件，刷新显示
        refreshConfigPathText();
        wxMessageBox(TR(IDS_MSG_EXPORT_SUCCESS), TR(IDS_APP_TITLE),
                     wxOK | wxICON_INFORMATION, this);
    } else {
        // 失败：状态不变（仍是 APPDATA），路径显示不动
        wxMessageBox(TR(IDS_MSG_EXPORT_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
    }
}

void SettingsPanel::onClearAdaptation(wxCommandEvent &) {
    LOG_DEBUG("settings: clear adaptation clicked");

    const int ret = wxMessageBox(TR(IDS_SETTINGS_MSG_CLEAR_ADAPT_CONFIRM),
                                 TR(IDS_APP_TITLE),
                                 wxOK | wxCANCEL | wxICON_QUESTION, this);
    if (ret != wxOK) return;

    ConfigManager::instance().setAdaptationPreference(AdaptationPreference::Ask);
    LOG_INFO("settings: adaptation preference reset to Ask");
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

    if (ConfigManager::instance().deletePortable()) {
        // 成功：路径已切回 APPDATA，刷新显示
        refreshConfigPathText();
        wxMessageBox(TR(IDS_SETTINGS_MSG_DELETE_PORTABLE_OK), TR(IDS_APP_TITLE),
                     wxOK | wxICON_INFORMATION, this);
    } else {
        // 失败：状态不变（仍是便携），路径显示不动
        wxMessageBox(TR(IDS_SETTINGS_MSG_DELETE_PORTABLE_FAILED), TR(IDS_APP_TITLE),
                     wxOK | wxICON_ERROR, this);
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::onHotkeySelectionChanged(wxListEvent &event) {
    const long row = m_hotkeyList->GetNextItem(-1, wxLIST_NEXT_ALL,
                                               wxLIST_STATE_SELECTED);
    m_btnModifyHotkey->Enable(row >= 0);
    event.Skip();
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::onHotkeyChecked(wxListEvent &event) {
    const long row = event.GetIndex();
    if (row < 0 || row >= kHotkeyCount) {
        event.Skip();
        return;
    }

    const int index = static_cast<int>(row);
    const bool checked = m_hotkeyList->IsItemChecked(row);

    // 幂等：编程式 CheckItem（refreshHotkeyList）可能触发本事件。
    // 读一遍 config 的当前值，一致就什么都不做。
    const auto idx = static_cast<std::size_t>(index);
    if (checked == ConfigManager::instance().config().hotkeyEnabled[idx]) {
        event.Skip();
        return;
    }

    LOG_DEBUG("settings: hotkey[{}] checked={}", index, checked);

    if (!m_hotkeyMgr.setEnabled(index, checked)) {
        // 失败一般是 enable 时键位被别的程序占用。
        // config 已经记录了用户意图（下次启动 registerAll 会重试），
        // 所以不回退 UI；失败状态通过 HotkeyManager::failedCount 反映。
        LOG_WARN("settings: hotkey[{}] setEnabled({}) failed (occupied?)",
                 index, checked);
    }

    event.Skip();
}
