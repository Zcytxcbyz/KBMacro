#include "ui/panels/SettingsPanel.hpp"

#include <filesystem>
#include <iterator>

#include <wx/msgdlg.h>
#include <wx/richmsgdlg.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include "common/Hotkey.hpp"
#include "common/HotkeyAction.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"
#include "managers/ConfigManager.hpp"
#include "managers/HotkeyManager.hpp"
#include "managers/LanguageManager.hpp"
#include "ui/DialogButtons.hpp"
#include "ui/ListHelpers.hpp"
#include "ui/MainFrame.hpp"
#include "ui/dialogs/HotkeyCaptureDialog.hpp"

namespace {
// Timer mode combo box indexes, matching the order of kTimerModeEntries
constexpr int kTimerModeSleep = 0;
constexpr int kTimerModeSpin = 1;
constexpr int kTimerModeHybrid = 2;

[[nodiscard]] int timerModeToIndex(const TimerMode mode) {
    switch (mode) {
        case TimerMode::Hybrid:
            return kTimerModeHybrid;
        case TimerMode::Spin:
            return kTimerModeSpin;
        case TimerMode::Sleep:
        default:
            return kTimerModeSleep;
    }
}

[[nodiscard]] TimerMode indexToTimerMode(const int index) {
    switch (index) {
        case kTimerModeHybrid:
            return TimerMode::Hybrid;
        case kTimerModeSpin:
            return TimerMode::Spin;
        default:
            return TimerMode::Sleep;
    }
}
} // namespace

SettingsPanel::SettingsPanel(wxWindow *parent, HotkeyManager &hotkeyMgr)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxHSCROLL),
      m_hotkeyMgr(hotkeyMgr) {
    buildUi();

    // The five groups are taller than one screen: vertical scrolling, one row ≈ 10 DIP per step;
    // no horizontal scrolling (0 = off); label text adapts via the sizer's growable col
    SetScrollRate(FromDIP(10), FromDIP(10));
    wxWindowBase::FitInside();

    refreshFromConfig();
    refreshHotkeyList();
    refreshConfigPathText();
}

void SettingsPanel::buildUi() {
    auto *root = new wxBoxSizer(wxVERTICAL);

    buildGeneralGroup(root);
    buildHotkeyGroup(root);
    buildConfigGroup(root);
    buildRecorderGroup(root);
    buildResetGroup(root);

    SetSizer(root);

    // ---- Event binding ----
    m_choiceLanguage->Bind(wxEVT_CHOICE, &SettingsPanel::onLanguageChanged, this);
    m_choiceTimerMode->Bind(wxEVT_CHOICE, &SettingsPanel::onTimerModeChanged, this);
    // wxMSW's wxSpinCtrl: clicking the arrows sends wxEVT_SPINCTRL, typing digits sends wxEVT_TEXT
    // (typing an out-of-range value sends neither; the focus-out clamp catches it). Bind both.
    m_spinHybridMs->Bind(wxEVT_SPINCTRL, &SettingsPanel::onHybridSpinChanged, this);
    m_spinHybridMs->Bind(wxEVT_TEXT, &SettingsPanel::onHybridSpinChanged, this);
    m_chkAlwaysOnTop->Bind(wxEVT_CHECKBOX, &SettingsPanel::onAlwaysOnTopToggled, this);
    m_btnModifyHotkey->Bind(wxEVT_BUTTON, &SettingsPanel::onModifyHotkey, this);
    m_btnExportConfig->Bind(wxEVT_BUTTON, &SettingsPanel::onExportConfig, this);
    m_btnClearAdaptation->Bind(wxEVT_BUTTON, &SettingsPanel::onClearAdaptation, this);
    m_btnClearSpinConfirm->Bind(wxEVT_BUTTON, &SettingsPanel::onClearSpinConfirm, this);
    m_btnResetDefaults->Bind(wxEVT_BUTTON, &SettingsPanel::onResetDefaults, this);
    m_btnDeletePortableConfig->Bind(wxEVT_BUTTON, &SettingsPanel::onDeletePortableConfig, this);
    // Recording group: the spins also need the SPINCTRL + TEXT double binding (typing goes through TEXT)
    m_spinRecMoveInterval->Bind(wxEVT_SPINCTRL, &SettingsPanel::onRecorderMoveIntervalChanged, this);
    m_spinRecMoveInterval->Bind(wxEVT_TEXT, &SettingsPanel::onRecorderMoveIntervalChanged, this);
    m_spinRecStartDelay->Bind(wxEVT_SPINCTRL, &SettingsPanel::onRecorderStartDelayChanged, this);
    m_spinRecStartDelay->Bind(wxEVT_TEXT, &SettingsPanel::onRecorderStartDelayChanged, this);
    m_spinRecMaxActions->Bind(wxEVT_SPINCTRL, &SettingsPanel::onRecorderMaxActionsChanged, this);
    m_spinRecMaxActions->Bind(wxEVT_TEXT, &SettingsPanel::onRecorderMaxActionsChanged, this);
    m_chkRecIgnoreInjected->Bind(wxEVT_CHECKBOX, &SettingsPanel::onRecorderIgnoreInjectedToggled, this);
}

// ============================================================
// Group 1: general
// ============================================================
void SettingsPanel::buildGeneralGroup(wxSizer *parent) {
    auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_SETTINGS_GROUP_GENERAL));
    auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

    auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
    form->AddGrowableCol(1, 1);

    // Language: iterate kLangs, index ↔ language is one-to-one
    m_choiceLanguage = new wxChoice(staticBox, wxID_ANY);
    for (const auto &lang : kLangs) {
        m_choiceLanguage->Append(wxString(lang.displayName.data(), lang.displayName.size()));
    }

    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_LANGUAGE)), 0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_choiceLanguage, 1, wxEXPAND);

    form->AddSpacer(0);
    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_HINT_RESTART)), 1, wxEXPAND);

    // Timer mode
    m_choiceTimerMode = new wxChoice(staticBox, wxID_ANY);
    m_choiceTimerMode->Append(TR(IDS_SETTINGS_TIMER_SLEEP));
    m_choiceTimerMode->Append(TR(IDS_SETTINGS_TIMER_SPIN));
    m_choiceTimerMode->Append(TR(IDS_SETTINGS_TIMER_HYBRID));

    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_TIMER)), 0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_choiceTimerMode, 1, wxEXPAND);

    // Hybrid mode: end-of-run busy-wait duration (editable only in Hybrid; see refreshFromConfig)
    m_spinHybridMs = new wxSpinCtrl(staticBox,
                                    wxID_ANY,
                                    wxEmptyString,
                                    wxDefaultPosition,
                                    FromDIP(wxSize(80, -1)),
                                    wxSP_ARROW_KEYS,
                                    kTimerHybridSpinMsMin,
                                    kTimerHybridSpinMsMax,
                                    kTimerHybridSpinMsDefault);
    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_HYBRID_SPIN)), 0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_spinHybridMs, 0, wxALIGN_CENTER_VERTICAL);

    box->Add(form, 0, wxEXPAND | wxALL, 8);

    // Always on top
    m_chkAlwaysOnTop = new wxCheckBox(staticBox, wxID_ANY, TR(IDS_SETTINGS_CHK_ONTOP));
    box->Add(m_chkAlwaysOnTop, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    parent->Add(box, 0, wxEXPAND | wxALL, 8);
}

// ============================================================
// Group 2: hotkeys
// ============================================================
void SettingsPanel::buildHotkeyGroup(wxSizer *parent) {
    auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_SETTINGS_GROUP_HOTKEYS));
    auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

    m_hotkeyList = new wxListCtrl(
            staticBox, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 150)), wxLC_REPORT | wxLC_SINGLE_SEL);
    m_hotkeyList->EnableCheckBoxes(true);
    m_hotkeyList->AppendColumn(TR(IDS_SETTINGS_LIST_COL_ACTION), wxLIST_FORMAT_LEFT, FromDIP(200));
    m_hotkeyList->AppendColumn(TR(IDS_SETTINGS_LIST_COL_HOTKEY), wxLIST_FORMAT_LEFT, FromDIP(150));

    // The list content is filled by refreshHotkeyList(), called once at the end of construction

    box->Add(m_hotkeyList, 0, wxEXPAND | wxALL, 8);

    auto *btnRow = new wxBoxSizer(wxHORIZONTAL);
    m_btnModifyHotkey = new wxButton(staticBox, wxID_ANY, TR(IDS_SETTINGS_BTN_MODIFY_HOTKEY));
    btnRow->Add(m_btnModifyHotkey, 0, wxALL, 4);
    btnRow->AddStretchSpacer(1);

    box->Add(btnRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

    parent->Add(box, 0, wxEXPAND | wxALL, 8);

    m_hotkeyList->Bind(wxEVT_LIST_ITEM_SELECTED, &SettingsPanel::onHotkeySelectionChanged, this);
    m_hotkeyList->Bind(wxEVT_LIST_ITEM_DESELECTED, &SettingsPanel::onHotkeySelectionChanged, this);
    m_hotkeyList->Bind(wxEVT_LIST_ITEM_CHECKED, &SettingsPanel::onHotkeyChecked, this); // ← added
    m_hotkeyList->Bind(wxEVT_LIST_ITEM_UNCHECKED, &SettingsPanel::onHotkeyChecked, this); // ← added

    // Still scroll the page when the mouse is over the list
    uifit::forwardWheelToPage(m_hotkeyList);

    // Initially nothing selected → button disabled
    m_btnModifyHotkey->Enable(false);
}

// ============================================================
// Group 3: config management
// ============================================================
void SettingsPanel::buildConfigGroup(wxSizer *parent) {
    auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_SETTINGS_GROUP_CONFIG));
    auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

    auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
    form->AddGrowableCol(1, 1);

    // The config path is long: ellipsize it instead of letting the label widen the whole page.
    m_configPathText = new wxStaticText(staticBox, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                        wxDefaultSize, wxST_ELLIPSIZE_MIDDLE | wxST_NO_AUTORESIZE);
    m_configPathText->SetMinSize(wxSize(FromDIP(280), -1));

    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_CONFIG_PATH)), 0, wxALIGN_CENTER_VERTICAL);
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
// Group: recording
// ============================================================
void SettingsPanel::buildRecorderGroup(wxSizer *parent) {
    auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_SETTINGS_GROUP_RECORD));
    auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

    auto *form = new wxFlexGridSizer(2, FromDIP(wxSize(8, 8)));
    form->AddGrowableCol(1, 1);

    // Mouse move sampling interval: 0 = no sampling
    m_spinRecMoveInterval = new wxSpinCtrl(staticBox,
                                           wxID_ANY,
                                           wxEmptyString,
                                           wxDefaultPosition,
                                           FromDIP(wxSize(80, -1)),
                                           wxSP_ARROW_KEYS,
                                           kRecordMoveIntervalMsMin,
                                           kRecordMoveIntervalMsMax,
                                           kRecordMoveIntervalMsDefault);
    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_REC_MOVE_INTERVAL)),
              0,
              wxALIGN_CENTER_VERTICAL);
    form->Add(m_spinRecMoveInterval, 0, wxALIGN_CENTER_VERTICAL);
    form->AddSpacer(0);
    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_HINT_REC_MOVE_INTERVAL)), 1, wxEXPAND);

    // Start delay: filters the keys of the start hotkey itself
    m_spinRecStartDelay = new wxSpinCtrl(staticBox,
                                         wxID_ANY,
                                         wxEmptyString,
                                         wxDefaultPosition,
                                         FromDIP(wxSize(80, -1)),
                                         wxSP_ARROW_KEYS,
                                         kRecordStartDelayMsMin,
                                         kRecordStartDelayMsMax,
                                         kRecordStartDelayMsDefault);
    form->Add(
            new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_REC_START_DELAY)), 0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_spinRecStartDelay, 0, wxALIGN_CENTER_VERTICAL);

    // Max actions: 0 = unlimited
    m_spinRecMaxActions = new wxSpinCtrl(staticBox,
                                         wxID_ANY,
                                         wxEmptyString,
                                         wxDefaultPosition,
                                         FromDIP(wxSize(110, -1)),
                                         wxSP_ARROW_KEYS,
                                         kRecordMaxActionsMin,
                                         kRecordMaxActionsMax,
                                         kRecordMaxActionsDefault);
    form->Add(
            new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_LABEL_REC_MAX_ACTIONS)), 0, wxALIGN_CENTER_VERTICAL);
    form->Add(m_spinRecMaxActions, 0, wxALIGN_CENTER_VERTICAL);
    form->AddSpacer(0);
    form->Add(new wxStaticText(staticBox, wxID_ANY, TR(IDS_SETTINGS_HINT_REC_MAX_ACTIONS)), 1, wxEXPAND);

    box->Add(form, 0, wxEXPAND | wxALL, 8);

    // Injected event filter (our own injections are never recorded; this covers "injected by other apps")
    m_chkRecIgnoreInjected = new wxCheckBox(staticBox, wxID_ANY, TR(IDS_SETTINGS_CHK_REC_IGNORE_INJECTED));
    box->Add(m_chkRecIgnoreInjected, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    parent->Add(box, 0, wxEXPAND | wxALL, 8);
}

// ============================================================
// Group 4: reset
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
// Sync from ConfigManager to the widgets
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::refreshFromConfig() {
    const auto &cfg = ConfigManager::instance().config();

    // Language: select the entry matching LanguageManager's current state
    const Lang cur = LanguageManager::instance().currentLang();
    for (std::size_t i = 0; i < std::size(kLangs); ++i) {
        if (kLangs[i].id == cur) {
            m_choiceLanguage->SetSelection(static_cast<int>(i));
            break;
        }
    }

    // Timer mode
    m_choiceTimerMode->SetSelection(timerModeToIndex(cfg.timerMode));

    // End-of-run busy-wait duration: only meaningful in Hybrid mode
    m_spinHybridMs->SetValue(cfg.timerHybridSpinMs);
    m_spinHybridMs->Enable(cfg.timerMode == TimerMode::Hybrid);

    // Always on top
    m_chkAlwaysOnTop->SetValue(cfg.alwaysOnTop);

    // Recording options only affect "the next recording start" (RecordOptions is built on start)
    m_spinRecMoveInterval->SetValue(cfg.recordMoveIntervalMs);
    m_spinRecStartDelay->SetValue(cfg.recordStartDelayMs);
    m_spinRecMaxActions->SetValue(cfg.recordMaxActions);
    m_chkRecIgnoreInjected->SetValue(cfg.recordIgnoreInjected);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::refreshHotkeyList() {
    m_hotkeyList->DeleteAllItems();

    const auto &cfg = ConfigManager::instance().config();
    for (std::size_t i = 0; i < std::size(kHotkeyActionResIds); ++i) {
        const long row = m_hotkeyList->InsertItem(static_cast<long>(i), TR(kHotkeyActionResIds[i]));
        const std::wstring s = utils::hotkeyToString(cfg.hotkeys[i]);
        m_hotkeyList->SetItem(row, 1, s.empty() ? TR(IDS_SETTINGS_HOTKEY_UNSET) : wxString(s));
        m_hotkeyList->CheckItem(row, cfg.hotkeyEnabled[i]);
    }

    // Column 0 must fit the longest action name: ru-RU is much wider than English,
    // and wxLIST_AUTOSIZE does not count the checkbox gutter added by EnableCheckBoxes().
    uifit::autoSizeColumns(m_hotkeyList, FromDIP(28));

    // Spread all 11 rows out, no scrollbar inside the list (the page handles scrolling)
    uifit::fitListToRows(m_hotkeyList);
}

void SettingsPanel::notifyHotkeyHints() {
    if (auto *frame = dynamic_cast<MainFrame *>(wxGetTopLevelParent(this)); frame != nullptr) {
        frame->refreshHotkeyHints();
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::refreshConfigPathText() {
    const auto &path = ConfigManager::instance().currentPath();
    m_configPathText->SetLabel(wxString(path.wstring()));
}

// ============================================================
// Event handling
// ============================================================

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void SettingsPanel::onLanguageChanged(wxCommandEvent &event) {
    const int sel = event.GetSelection();
    if (sel < 0 || sel >= static_cast<int>(std::size(kLangs))) return;

    const Lang newLang = kLangs[sel].id;
    const Lang curLang = LanguageManager::instance().currentLang();

    // User switched back to the current language: do nothing
    if (newLang == curLang) return;

    // Three-way choice: restart now / restart later / cancel.
    // Not wxMessageDialog: its parent classes only provide SetOKLabel / SetYesNoLabels /
    // SetYesNoCancelLabels, no SetCancelLabel, so the cancel button would always read "Cancel".
    wxDialog dlg(this, wxID_ANY, TR(IDS_APP_TITLE));

    auto *text = new wxStaticText(&dlg, wxID_ANY, TR(IDS_SETTINGS_MSG_RESTART_ASK));
    auto *btnNow = new wxButton(&dlg, wxID_YES, TR(IDS_SETTINGS_BTN_RESTART_NOW));
    auto *btnLater = new wxButton(&dlg, wxID_NO, TR(IDS_SETTINGS_BTN_RESTART_LATER));
    auto *btnCancel = new wxButton(&dlg, wxID_CANCEL, TR(IDS_COMMON_BTN_CANCEL));

    auto *btnRow = new wxBoxSizer(wxHORIZONTAL);
    btnRow->Add(btnNow, 0, wxRIGHT, FromDIP(8));
    btnRow->Add(btnLater, 0, wxRIGHT, FromDIP(8));
    btnRow->Add(btnCancel, 0);

    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(text, 0, wxALL, FromDIP(16));
    root->Add(btnRow, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    // wxDialog auto-calls EndModal only for the affirmative button (wxID_OK) and the escape button
    // (wxID_CANCEL); wxID_YES / wxID_NO are not included, so bind them explicitly or clicks do nothing.
    btnNow->Bind(wxEVT_BUTTON, [&dlg](wxCommandEvent &) { dlg.EndModal(wxID_YES); });
    btnLater->Bind(wxEVT_BUTTON, [&dlg](wxCommandEvent &) { dlg.EndModal(wxID_NO); });
    btnCancel->Bind(wxEVT_BUTTON, [&dlg](wxCommandEvent &) { dlg.EndModal(wxID_CANCEL); });

    dlg.SetSizerAndFit(root);
    dlg.CentreOnParent();
    btnNow->SetDefault(); // Enter = restart now
    const int ret = dlg.ShowModal();

    if (ret == wxID_CANCEL) {
        // Revert the widget to the current language
        for (std::size_t i = 0; i < std::size(kLangs); ++i) {
            if (kLangs[i].id == curLang) {
                m_choiceLanguage->SetSelection(static_cast<int>(i));
                break;
            }
        }
        LOG_DEBUG("settings: language change cancelled");
        return;
    }

    // Persist (an explicit user choice = remember it)
    ConfigManager::instance().setLanguage(kLangs[sel].code);
    LOG_INFO("settings: language changed to '{}'", kLangs[sel].code);

    if (ret == wxID_YES) {
        // Restart now
        const wxString exePath = wxStandardPaths::Get().GetExecutablePath();
        LOG_INFO("settings: restarting immediately, exe={}", utils::ToUtf8(exePath.ToStdWstring()));

        // The path below goes through ExitMainLoop, which does not fire wxEVT_CLOSE_WINDOW,
        // and the new process reads the config right away. So save the window state explicitly.
        // The order matters: save → yield hotkeys → launch the new process → exit the old one.
        if (auto *frame = dynamic_cast<MainFrame *>(wxGetTopLevelParent(this))) {
            frame->saveWindowState();
        }

        // These 11 global hotkeys must be yielded before restarting: RegisterHotKey is exclusive,
        // and the new process registers within tens of milliseconds; if the old process still holds
        // them, all registrations fail (1409). Unregister before launching, the order must not change.
        m_hotkeyMgr.unregisterAll();

        if (wxExecute(wxString::Format("\"%s\"", exePath), wxEXEC_ASYNC) > 0) {
            // New process started.
            // Use ExitMainLoop rather than Close(true):
            //   Close is a "request to close the window" handled by the event loop;
            //   calling it inside an event handler is easily deferred or ignored (old window stays).
            //   ExitMainLoop exits the event loop directly, guaranteeing the old process quits.
            // OnExit is still called normally, so logs are flushed.
            wxTheApp->ExitMainLoop(); // NOLINT(*-pro-type-static-cast-downcast)
        } else {
            // Failed to launch the new process: do not exit, let the user keep using this instance
            // The config is already saved, so the next manual start still uses the new language.
            // But we just unregistered the hotkeys, so register them back or this instance loses them.
            LOG_ERROR("settings: failed to launch new instance");
            m_hotkeyMgr.registerAll();
            LOG_WARN("settings: hotkeys re-registered after restart failure, failed={}", m_hotkeyMgr.failedCount());
        }
    }
    // wxID_NO = restart later: do nothing, the config is already saved
}

void SettingsPanel::onTimerModeChanged(const wxCommandEvent &event) {
    const int sel = event.GetSelection();
    if (sel < 0) return;

    LOG_DEBUG("settings: timer mode changed, index={}", sel);

    const TimerMode newMode = indexToTimerMode(sel);
    auto &cfgMgr = ConfigManager::instance();

    // Spin and Hybrid both burn CPU (Hybrid only at the end); they share one warning
    if (newMode != TimerMode::Sleep && !cfgMgr.config().flags.spinWarningConfirmed) {
        // Use wxRichMessageDialog: it has a checkbox so the user can ask to remember the choice
        wxRichMessageDialog dlg(this,
                                TR(IDS_SETTINGS_MSG_SPIN_WARNING),
                                TR(IDS_APP_TITLE),
                                wxYES_NO | wxICON_WARNING); // wxOK|wxCANCEL → wxYES_NO
        // ↓ added this line so both buttons follow the language
        dlg.SetYesNoLabels(uibtn::label(IDS_COMMON_BTN_YES), uibtn::label(IDS_COMMON_BTN_NO));
        dlg.ShowCheckBox(TR(IDS_SETTINGS_CHK_SPIN_DONT_ASK));
        const int ret = dlg.ShowModal();

        if (ret != wxID_YES) {
            // User cancelled: revert the widget to the active mode, leave the config untouched
            m_choiceTimerMode->SetSelection(timerModeToIndex(cfgMgr.config().timerMode));
            return;
        }

        // Remember only when "don't ask again" is checked; otherwise the next switch prompts again
        if (dlg.IsCheckBoxChecked()) {
            LOG_DEBUG("settings: spin warning marked as acknowledged");
            cfgMgr.setSpinWarningConfirmed(true);
        }
    }

    // The end-of-run busy-wait duration only matters for Hybrid
    m_spinHybridMs->Enable(newMode == TimerMode::Hybrid);

    cfgMgr.setTimerMode(newMode);

    // Tell the main window to rebuild the timer instance: it only affects tasks started after this
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
    (void)event; // the two event types differ, so read the widget value instead of event.GetInt()

    const int ms = m_spinHybridMs->GetValue();
    auto &cfgMgr = ConfigManager::instance();

    // Typing digits fires wxEVT_TEXT on every keystroke; do nothing when the value is unchanged
    // ("25" arrives as 2, then 25; only a real change persists + rebuilds).
    if (ms == cfgMgr.config().timerHybridSpinMs) return;

    LOG_DEBUG("settings: hybrid spin tail = {}ms", ms);
    cfgMgr.setTimerHybridSpinMs(ms);

    // The threshold is a HybridTimer constructor argument, so it takes effect only after a rebuild
    if (auto *frame = dynamic_cast<MainFrame *>(wxGetTopLevelParent(this))) {
        frame->rebuildTimer();
    }
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::onRecorderMoveIntervalChanged(wxCommandEvent &event) {
    (void)event; // SPINCTRL and TEXT share this handler, so always read the widget value

    const int ms = m_spinRecMoveInterval->GetValue();
    auto &cfgMgr = ConfigManager::instance();

    // Typing fires once per keystroke; do nothing when the value is unchanged
    if (ms == cfgMgr.config().recordMoveIntervalMs) return;

    LOG_DEBUG("settings: record move interval = {}ms", ms);
    cfgMgr.setRecordMoveIntervalMs(ms);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
// ReSharper disable once CppParameterMayBeConstPtrOrRef
void SettingsPanel::onRecorderIgnoreInjectedToggled( // NOLINT(*-convert-member-functions-to-static)
        wxCommandEvent &event) { // NOLINT(*-convert-member-functions-to-static)
    const bool on = event.IsChecked();
    auto &cfgMgr = ConfigManager::instance();

    if (on == cfgMgr.config().recordIgnoreInjected) return;

    LOG_DEBUG("settings: record ignore injected = {}", on);
    cfgMgr.setRecordIgnoreInjected(on);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
// ReSharper disable once CppParameterMayBeConstPtrOrRef
// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::onRecorderStartDelayChanged(wxCommandEvent &event) { // NOLINT(*-convert-member-functions-to-static)
    (void)event;

    const int ms = m_spinRecStartDelay->GetValue();
    auto &cfgMgr = ConfigManager::instance();

    if (ms == cfgMgr.config().recordStartDelayMs) return;

    LOG_DEBUG("settings: record start delay = {}ms", ms);
    cfgMgr.setRecordStartDelayMs(ms);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
// ReSharper disable once CppParameterMayBeConstPtrOrRef
// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::onRecorderMaxActionsChanged(wxCommandEvent &event) {
    (void)event;

    const int n = m_spinRecMaxActions->GetValue();
    auto &cfgMgr = ConfigManager::instance();

    if (n == cfgMgr.config().recordMaxActions) return;

    LOG_DEBUG("settings: record max actions = {}", n);
    cfgMgr.setRecordMaxActions(n);
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void SettingsPanel::onAlwaysOnTopToggled(wxCommandEvent &event) {
    const bool on = event.IsChecked();
    LOG_DEBUG("settings: always on top = {}", on);

    // Persist
    ConfigManager::instance().setAlwaysOnTop(on);

    // Apply to the window immediately
    auto *frame = wxDynamicCast(wxGetTopLevelParent(this), wxFrame); // NOLINT(*-pro-type-static-cast-downcast)
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

    // On MSW, SetWindowStyleFlag toggling wxSTAY_ON_TOP does not reorder the z-order automatically,
    // so Raise once to make the window state take effect immediately
    frame->Raise();
}

void SettingsPanel::onModifyHotkey(wxCommandEvent &) {
    const long row = m_hotkeyList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0) {
        LOG_DEBUG("settings: modify hotkey clicked, but no row selected");
        return;
    }

    const auto index = static_cast<int>(row);
    const Hotkey current = ConfigManager::instance().config().hotkeys[index];

    LOG_DEBUG("settings: modify hotkey[{}], current='{}'", index, utils::ToUtf8(utils::hotkeyToString(current)));

    // Conflict check: the closure captures index to exclude this row
    HotkeyCaptureDialog dlg(this,
                            TR(IDS_SETTINGS_BTN_MODIFY_HOTKEY),
                            current,
                            /*requireMainKey=*/true,
                            [this, index](const Hotkey &hk) { return m_hotkeyMgr.isConflicting(hk, index); });

    if (dlg.ShowModal() != wxID_OK) {
        LOG_DEBUG("settings: hotkey[{}] modify cancelled", index);
        return;
    }

    const Hotkey wanted = dlg.getHotkey();
    const std::string wantedStr = utils::ToUtf8(utils::hotkeyToString(wanted));

    // setHotkey does: conflict check → unregister old → persist → register new
    // Conflicts prompt here (HotkeyCaptureDialog checks first); a failure here usually means "occupied"
    if (!m_hotkeyMgr.setHotkey(index, wanted)) {
        LOG_WARN("settings: hotkey[{}] set to '{}' failed (occupied?)", index, wantedStr);
    } else {
        LOG_INFO("settings: hotkey[{}] set to '{}'", index, wantedStr);
    }

    refreshHotkeyList();
    notifyHotkeyHints();
}

void SettingsPanel::onExportConfig(wxCommandEvent &) {
    LOG_DEBUG("settings: export config clicked");

    if (ConfigManager::instance().exportToPortable()) {
        // Success: the path switched to the portable file, refresh the display
        refreshConfigPathText();
        uibtn::messageBox(TR(IDS_MSG_EXPORT_SUCCESS), TR(IDS_APP_TITLE), wxOK | wxICON_INFORMATION, this);
    } else {
        // Failure: state unchanged (still APPDATA), leave the path display alone
        uibtn::messageBox(TR(IDS_MSG_EXPORT_FAILED), TR(IDS_APP_TITLE), wxOK | wxICON_ERROR, this);
    }
}

void SettingsPanel::onClearAdaptation(wxCommandEvent &) {
    LOG_DEBUG("settings: clear adaptation clicked");

    const int ret = uibtn::messageBox(
            TR(IDS_SETTINGS_MSG_CLEAR_ADAPT_CONFIRM), TR(IDS_APP_TITLE), wxYES_NO | wxICON_QUESTION, this);
    if (ret != wxID_YES) return;

    ConfigManager::instance().setAdaptationPreference(AdaptationPreference::Ask);
    LOG_INFO("settings: adaptation preference reset to Ask");
}

void SettingsPanel::onClearSpinConfirm(wxCommandEvent &) {
    LOG_DEBUG("settings: clear spin confirmation clicked");

    const int ret = uibtn::messageBox(
            TR(IDS_SETTINGS_MSG_CLEAR_SPIN_CONFIRM), TR(IDS_APP_TITLE), wxYES_NO | wxICON_QUESTION, this);
    if (ret != wxID_YES) return;

    ConfigManager::instance().clearSpinWarning();
}

void SettingsPanel::onResetDefaults(wxCommandEvent &) {
    LOG_DEBUG("settings: reset defaults clicked");

    const int ret =
            uibtn::messageBox(TR(IDS_SETTINGS_MSG_RESET_CONFIRM), TR(IDS_APP_TITLE), wxYES_NO | wxICON_QUESTION, this);
    if (ret != wxID_YES) return;

    ConfigManager::instance().resetToDefaults();

    // After the reset: every hotkey in the config is empty, so register them again
    // HotkeyManager::registerAll walks the config and registers each one (empty hotkeys are skipped)
    m_hotkeyMgr.registerAll();

    // Refresh all widgets back to their defaults
    refreshFromConfig();
    refreshHotkeyList();
    notifyHotkeyHints();
    refreshConfigPathText();

    // Always-on-top also returns to its default (false)
    if (auto *frame = wxDynamicCast(wxGetTopLevelParent(this), wxFrame)) { // NOLINT(*-pro-type-static-cast-downcast)
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
        uibtn::messageBox(TR(IDS_SETTINGS_MSG_DELETE_NO_FILE), TR(IDS_APP_TITLE), wxOK | wxICON_INFORMATION, this);
        return;
    }

    const wxString msg =
            TR(IDS_SETTINGS_MSG_DELETE_CONFIRM) + L"\n\n" + ConfigManager::instance().currentPath().wstring();
    const int ret = uibtn::messageBox(msg, TR(IDS_APP_TITLE), wxYES_NO | wxICON_QUESTION, this);
    if (ret != wxID_YES) return;

    if (ConfigManager::instance().deletePortable()) {
        // Success: the path switched back to APPDATA, refresh the display
        refreshConfigPathText();
        uibtn::messageBox(TR(IDS_SETTINGS_MSG_DELETE_PORTABLE_OK), TR(IDS_APP_TITLE), wxOK | wxICON_INFORMATION, this);
    } else {
        // Failure: state unchanged (still portable), leave the path display alone
        uibtn::messageBox(TR(IDS_SETTINGS_MSG_DELETE_PORTABLE_FAILED), TR(IDS_APP_TITLE), wxOK | wxICON_ERROR, this);
    }
}

// ReSharper disable once CppMemberFunctionMayBeConst
void SettingsPanel::onHotkeySelectionChanged(wxListEvent &event) {
    const long row = m_hotkeyList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
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

    // Idempotent: the programmatic CheckItem (refreshHotkeyList) may fire this event.
    // Read the current config value; do nothing when it already matches.
    const auto idx = static_cast<std::size_t>(index);
    if (checked == ConfigManager::instance().config().hotkeyEnabled[idx]) {
        event.Skip();
        return;
    }

    LOG_DEBUG("settings: hotkey[{}] checked={}", index, checked);

    if (!m_hotkeyMgr.setEnabled(index, checked)) {
        // A failure usually means the key is taken by another app when enabling.
        // The config already records the user's intent (the next start retries via registerAll),
        // so the UI is not reverted; the failure shows up through HotkeyManager::failedCount.
        LOG_WARN("settings: hotkey[{}] setEnabled({}) failed (occupied?)", index, checked);
    }

    notifyHotkeyHints();

    event.Skip();
}
