#include "ui/dialogs/ActionEditDialog.hpp"

#include <wx/statbox.h>

#include "ui/DialogButtons.hpp"
#include "common/Hotkey.hpp"
#include "common/Log.hpp"
#include "managers/LanguageManager.hpp"
#include "ui/ScreenCaptureOverlay.hpp"
#include "ui/dialogs/HotkeyCaptureDialog.hpp"

namespace {
    // PropertyGrid internal property names (keys), unrelated to the UI labels
    constexpr auto kPropX        = "x";
    constexpr auto kPropY        = "y";
    constexpr auto kPropDelay    = "delayMs";
    constexpr auto kPropHold     = "holdMs";
    constexpr auto kPropButton   = "button";
    constexpr auto kPropWheelDir = "wheelDir";
    constexpr auto kPropWheelSteps = "wheelSteps";
    constexpr auto kPropKey      = "key";
    constexpr auto kPropKeyEvent = "keyEvent";

    // build the mouse button dropdown
    wxPGChoices makeButtonChoices() {
        wxPGChoices c;
        c.Add(TR(IDS_CLICKER_TYPE_LEFT),   static_cast<int>(MouseButton::Left));
        c.Add(TR(IDS_CLICKER_TYPE_RIGHT),  static_cast<int>(MouseButton::Right));
        c.Add(TR(IDS_CLICKER_TYPE_MIDDLE), static_cast<int>(MouseButton::Middle));
        c.Add(TR(IDS_CLICKER_TYPE_SIDE1),  static_cast<int>(MouseButton::Side1));
        c.Add(TR(IDS_CLICKER_TYPE_SIDE2),  static_cast<int>(MouseButton::Side2));
        return c;
    }

    wxPGChoices makeWheelDirChoices() {
        wxPGChoices c;
        c.Add(TR(IDS_ACTION_WHEEL_UP),    static_cast<int>(WheelDirection::Up));
        c.Add(TR(IDS_ACTION_WHEEL_DOWN),  static_cast<int>(WheelDirection::Down));
        c.Add(TR(IDS_ACTION_WHEEL_LEFT),  static_cast<int>(WheelDirection::Left));
        c.Add(TR(IDS_ACTION_WHEEL_RIGHT), static_cast<int>(WheelDirection::Right));
        return c;
    }

    wxPGChoices makeKeyEventChoices() {
        wxPGChoices c;
        c.Add(TR(IDS_ACTION_KEY_DOWN), static_cast<int>(KeyEventType::Down));
        c.Add(TR(IDS_ACTION_KEY_UP),   static_cast<int>(KeyEventType::Up));
        return c;
    }

    // action types that carry a mouse button
    [[nodiscard]] bool hasButton(const ActionType t) noexcept {
        return t == ActionType::MouseClick || t == ActionType::MouseDown ||
               t == ActionType::MouseUp;
    }

    // action types that carry coordinates; both ends of a drag need them too
    [[nodiscard]] bool hasCoordinates(const ActionType t) noexcept {
        return t == ActionType::MouseMove || hasButton(t);
    }
}

ActionEditDialog::ActionEditDialog(wxWindow *parent, const wxString &title,
                                   const Action &action)
    : wxDialog(parent, wxID_ANY, title),
      m_action(action) {
    buildUi();
}

void ActionEditDialog::buildUi() {
    auto *root = new wxBoxSizer(wxVERTICAL);

    // ---- action type ----
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, TR(IDS_ACTION_LABEL_TYPE)),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

        m_choiceType = new wxChoice(this, wxID_ANY);
        m_choiceType->Append(TR(IDS_ACTION_TYPE_MOUSE_MOVE));
        m_choiceType->Append(TR(IDS_ACTION_TYPE_MOUSE_CLICK));
        m_choiceType->Append(TR(IDS_ACTION_TYPE_MOUSE_WHEEL));
        m_choiceType->Append(TR(IDS_ACTION_TYPE_KEYBOARD));
        m_choiceType->Append(TR(IDS_ACTION_TYPE_MOUSE_DOWN));
        m_choiceType->Append(TR(IDS_ACTION_TYPE_MOUSE_UP));
        m_choiceType->Append(TR(IDS_ACTION_TYPE_DELAY));
        m_choiceType->SetSelection(static_cast<int>(m_action.type));   // order must match the enum

        row->Add(m_choiceType, 1, wxALIGN_CENTER_VERTICAL);
        root->Add(row, 0, wxEXPAND | wxALL, 12);
    }

    // ---- relative coordinates (meaningful only for types with coordinates) ----
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        m_chkRelative = new wxCheckBox(this, wxID_ANY, TR(IDS_ACTION_CHK_RELATIVE));
        m_chkRelative->SetValue(m_action.relative);
        row->Add(m_chkRelative, 0, wxALIGN_CENTER_VERTICAL);
        root->Add(row, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    }

    // ---- parameter table (PropertyGrid) ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_ACTION_GROUP_PARAMS));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        m_grid = new wxPropertyGrid(staticBox, wxID_ANY,
                                    wxDefaultPosition, FromDIP(wxSize(420, 200)),
                                    wxPG_DEFAULT_STYLE | wxPG_SPLITTER_AUTO_CENTER);
        box->Add(m_grid, 1, wxEXPAND | wxALL, 8);

        root->Add(box, 1, wxEXPAND | wxLEFT | wxRIGHT, 12);
    }

    // ---- aux buttons ----
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        m_btnCapturePos = new wxButton(this, wxID_ANY, TR(IDS_ACTION_DIALOG_BTN_CAPTURE));
        m_btnSetKey = new wxButton(this, wxID_ANY, TR(IDS_ACTION_DIALOG_BTN_SET_KEY));
        row->Add(m_btnCapturePos, 0, wxRIGHT, 8);
        row->Add(m_btnSetKey, 0);
        row->AddStretchSpacer(1);
        root->Add(row, 0, wxEXPAND | wxALL, 12);
    }

    // ---- standard button row ----
    root->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);

    uibtn::localize(this);

    SetSizerAndFit(root);
    SetMinSize(FromDIP(wxSize(500, 420)));

    // ---- event bindings ----
    m_choiceType->Bind(wxEVT_CHOICE, &ActionEditDialog::onTypeChanged, this);
    m_chkRelative->Bind(wxEVT_CHECKBOX, &ActionEditDialog::onRelativeToggled, this);
    m_btnCapturePos->Bind(wxEVT_BUTTON, &ActionEditDialog::onCapturePosition, this);
    m_btnSetKey->Bind(wxEVT_BUTTON, &ActionEditDialog::onSetKey, this);
    Bind(wxEVT_BUTTON, &ActionEditDialog::onOk, this, wxID_OK);

    // ---- initial build ----
    rebuildParamGrid();
    refreshAuxButtons();
}

// ============================================================
// action type change: keep delayMs, reset the other fields
// ============================================================
void ActionEditDialog::onTypeChanged(wxCommandEvent &) {
    const int sel = m_choiceType->GetSelection();
    if (sel < 0) return;

    const auto newType = static_cast<ActionType>(sel);
    if (newType == m_action.type) return;

    // read current values from the old grid before switching (mainly delayMs)
    collectFromGrid();

    const int savedDelay = m_action.delayMs;
    m_action = Action{};
    m_action.type = newType;
    m_action.delayMs = savedDelay;

    rebuildParamGrid();
    refreshAuxButtons();
}

// ============================================================
// toggle "relative coordinates": X/Y labels change, values are kept
// ============================================================
void ActionEditDialog::onRelativeToggled(wxCommandEvent &) {
    collectFromGrid();                                   // read back current values first
    m_action.relative = m_chkRelative->GetValue();
    rebuildParamGrid();                                  // labels change from "X/Y" to "X offset/Y offset"
    refreshAuxButtons();
}

// ============================================================
// rebuild parameter table
// ============================================================
void ActionEditDialog::rebuildParamGrid() {
    m_grid->Clear();

    const bool withButton = hasButton(m_action.type);
    const bool withCoords = hasCoordinates(m_action.type);

    // fixed order: button -> coordinates -> wheel -> key -> delay -> hold.
    // every type's own field order fits this sequence, so rows don't jump when the type changes.
    if (withButton) {
        wxPGChoices buttonChoices = makeButtonChoices();
        m_grid->Append(new wxEnumProperty(TR(IDS_ACTION_PARAM_BUTTON), kPropButton, buttonChoices,
                                          static_cast<int>(m_action.button)));
    }

    if (withCoords) {
        const auto labelX = m_action.relative ? TR(IDS_ACTION_PARAM_X_REL)
                                              : TR(IDS_ACTION_PARAM_X);
        const auto labelY = m_action.relative ? TR(IDS_ACTION_PARAM_Y_REL)
                                              : TR(IDS_ACTION_PARAM_Y);
        m_grid->Append(new wxIntProperty(labelX, kPropX, m_action.x));
        m_grid->Append(new wxIntProperty(labelY, kPropY, m_action.y));
    }

    if (m_action.type == ActionType::MouseWheel) {
        wxPGChoices dirChoices = makeWheelDirChoices();
        m_grid->Append(new wxEnumProperty(TR(IDS_ACTION_PARAM_WHEEL_DIR), kPropWheelDir,
                                          dirChoices,
                                          static_cast<int>(m_action.wheelDirection)));
        m_grid->Append(new wxIntProperty(TR(IDS_ACTION_PARAM_WHEEL_STEPS), kPropWheelSteps,
                                         m_action.wheelSteps));
    }

    if (m_action.type == ActionType::Keyboard) {
        auto *keyProp = new wxStringProperty(TR(IDS_ACTION_PARAM_KEY), kPropKey,
                                             wxString(utils::hotkeyToString(m_action.hotkey)));
        keyProp->ChangeFlag(wxPG_PROP_READONLY, true);
        m_grid->Append(keyProp);

        wxPGChoices eventChoices = makeKeyEventChoices();
        m_grid->Append(new wxEnumProperty(TR(IDS_ACTION_PARAM_KEY_EVENT), kPropKeyEvent,
                                          eventChoices,
                                          static_cast<int>(m_action.keyEvent)));
    }

    m_grid->Append(new wxIntProperty(TR(IDS_ACTION_PARAM_DELAY), kPropDelay, m_action.delayMs));

    if (m_action.type == ActionType::MouseClick) {
        m_grid->Append(new wxIntProperty(TR(IDS_ACTION_PARAM_HOLD), kPropHold, m_action.holdMs));
    }
}

// ============================================================
// read fields back from the grid (only properties present for the current type)
// ============================================================
void ActionEditDialog::collectFromGrid() {
    const auto get = [this](const char *name) -> int {
        if (auto *p = m_grid->GetPropertyByName(name)) {
            return p->GetValue().GetLong();
        }
        return 0;
    };

    if (hasButton(m_action.type)) {
        m_action.button = static_cast<MouseButton>(get(kPropButton));
    }

    if (hasCoordinates(m_action.type)) {
        m_action.x = get(kPropX);
        m_action.y = get(kPropY);
    }

    if (m_action.type == ActionType::MouseWheel) {
        m_action.wheelDirection = static_cast<WheelDirection>(get(kPropWheelDir));
        m_action.wheelSteps = get(kPropWheelSteps);
    }

    if (m_action.type == ActionType::Keyboard) {
        m_action.keyEvent = static_cast<KeyEventType>(get(kPropKeyEvent));
        // hotkey is updated only by the "set key" button; the grid just displays it
    }

    m_action.delayMs = get(kPropDelay);

    if (m_action.type == ActionType::MouseClick) {
        m_action.holdMs = get(kPropHold);
    }
}

// ============================================================
// aux button enabling
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void ActionEditDialog::refreshAuxButtons() {
    const bool withCoords = hasCoordinates(m_action.type);
    const bool absolute = withCoords && !m_action.relative;

    m_chkRelative->Enable(withCoords);
    m_chkRelative->SetValue(m_action.relative);

    // position capture only makes sense for absolute coordinates (relative actions need an offset)
    m_btnCapturePos->Enable(absolute);
    m_btnSetKey->Enable(m_action.type == ActionType::Keyboard);
}

// ============================================================
// capture position: overlay -> fill x/y back + update grid
// ============================================================
void ActionEditDialog::onCapturePosition(wxCommandEvent &) {
    new ScreenCaptureOverlay(this,
        [this](const bool captured, const wxPoint pos) {
            if (!captured) return;
            m_action.x = pos.x;
            m_action.y = pos.y;
            m_grid->GetPropertyByName(kPropX)->SetValue(pos.x);
            m_grid->GetPropertyByName(kPropY)->SetValue(pos.y);
        });
}

// ============================================================
// set key: hotkey capture -> update hotkey + grid display
// ============================================================
void ActionEditDialog::onSetKey(wxCommandEvent &) {
    HotkeyCaptureDialog dlg(this, TR(IDS_ACTION_DIALOG_BTN_SET_KEY),
                            m_action.hotkey, /*requireMainKey=*/false);
    if (dlg.ShowModal() != wxID_OK) return;

    m_action.hotkey = dlg.getHotkey();
    m_grid->GetPropertyByName(kPropKey)
          ->SetValue(wxString(utils::hotkeyToString(m_action.hotkey)));
}

// ============================================================
// OK: collect from the grid first, then let the dialog close normally
// ============================================================
void ActionEditDialog::onOk(wxCommandEvent &event) {
    collectFromGrid();
    LOG_DEBUG("action dialog: ok, type={}", static_cast<int>(m_action.type));
    event.Skip();
}
