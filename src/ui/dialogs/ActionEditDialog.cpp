#include "ui/dialogs/ActionEditDialog.hpp"

#include <wx/statbox.h>

#include "ui/DialogButtons.hpp"
#include "common/Hotkey.hpp"
#include "common/Log.hpp"
#include "managers/LanguageManager.hpp"
#include "ui/ScreenCaptureOverlay.hpp"
#include "ui/dialogs/HotkeyCaptureDialog.hpp"

namespace {
    // PropertyGrid 内部属性名（key），与界面 label 无关
    constexpr auto kPropX        = "x";
    constexpr auto kPropY        = "y";
    constexpr auto kPropDelay    = "delayMs";
    constexpr auto kPropHold     = "holdMs";
    constexpr auto kPropButton   = "button";
    constexpr auto kPropWheelDir = "wheelDir";
    constexpr auto kPropWheelSteps = "wheelSteps";
    constexpr auto kPropKey      = "key";
    constexpr auto kPropKeyEvent = "keyEvent";

    // 鼠标按键下拉的构造
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

    // 带鼠标按键的动作类型
    [[nodiscard]] bool hasButton(const ActionType t) noexcept {
        return t == ActionType::MouseClick || t == ActionType::MouseDown ||
               t == ActionType::MouseUp;
    }

    // 带坐标的动作类型：拖动两端也要坐标
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

    // ---- 动作类型 ----
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
        m_choiceType->SetSelection(static_cast<int>(m_action.type));   // 顺序必须与 enum 一致

        row->Add(m_choiceType, 1, wxALIGN_CENTER_VERTICAL);
        root->Add(row, 0, wxEXPAND | wxALL, 12);
    }

    // ---- 相对坐标（只对带坐标的类型有意义）----
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        m_chkRelative = new wxCheckBox(this, wxID_ANY, TR(IDS_ACTION_CHK_RELATIVE));
        m_chkRelative->SetValue(m_action.relative);
        row->Add(m_chkRelative, 0, wxALIGN_CENTER_VERTICAL);
        root->Add(row, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    }

    // ---- 参数表（PropertyGrid） ----
    {
        auto *staticBox = new wxStaticBox(this, wxID_ANY, TR(IDS_ACTION_GROUP_PARAMS));
        auto *box = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        m_grid = new wxPropertyGrid(staticBox, wxID_ANY,
                                    wxDefaultPosition, FromDIP(wxSize(420, 200)),
                                    wxPG_DEFAULT_STYLE | wxPG_SPLITTER_AUTO_CENTER);
        box->Add(m_grid, 1, wxEXPAND | wxALL, 8);

        root->Add(box, 1, wxEXPAND | wxLEFT | wxRIGHT, 12);
    }

    // ---- 辅助按钮 ----
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        m_btnCapturePos = new wxButton(this, wxID_ANY, TR(IDS_ACTION_DIALOG_BTN_CAPTURE));
        m_btnSetKey = new wxButton(this, wxID_ANY, TR(IDS_ACTION_DIALOG_BTN_SET_KEY));
        row->Add(m_btnCapturePos, 0, wxRIGHT, 8);
        row->Add(m_btnSetKey, 0);
        row->AddStretchSpacer(1);
        root->Add(row, 0, wxEXPAND | wxALL, 12);
    }

    // ---- 标准按钮行 ----
    root->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);

    uibtn::localize(this);

    SetSizerAndFit(root);
    SetMinSize(FromDIP(wxSize(500, 420)));

    // ---- 事件绑定 ----
    m_choiceType->Bind(wxEVT_CHOICE, &ActionEditDialog::onTypeChanged, this);
    m_chkRelative->Bind(wxEVT_CHECKBOX, &ActionEditDialog::onRelativeToggled, this);
    m_btnCapturePos->Bind(wxEVT_BUTTON, &ActionEditDialog::onCapturePosition, this);
    m_btnSetKey->Bind(wxEVT_BUTTON, &ActionEditDialog::onSetKey, this);
    Bind(wxEVT_BUTTON, &ActionEditDialog::onOk, this, wxID_OK);

    // ---- 初始构建 ----
    rebuildParamGrid();
    refreshAuxButtons();
}

// ============================================================
// 动作类型切换：保留 delayMs，其余字段重置
// ============================================================
void ActionEditDialog::onTypeChanged(wxCommandEvent &) {
    const int sel = m_choiceType->GetSelection();
    if (sel < 0) return;

    const auto newType = static_cast<ActionType>(sel);
    if (newType == m_action.type) return;

    // 切换类型前先从旧网格读回当前值（主要为 delayMs）
    collectFromGrid();

    const int savedDelay = m_action.delayMs;
    m_action = Action{};
    m_action.type = newType;
    m_action.delayMs = savedDelay;

    rebuildParamGrid();
    refreshAuxButtons();
}

// ============================================================
// 勾选/取消"相对坐标"：X/Y 的标签跟着变，值保留
// ============================================================
void ActionEditDialog::onRelativeToggled(wxCommandEvent &) {
    collectFromGrid();                                   // 先读回当前值
    m_action.relative = m_chkRelative->GetValue();
    rebuildParamGrid();                                  // 标签要从"X/Y"变"X 偏移/Y 偏移"
    refreshAuxButtons();
}

// ============================================================
// 重建参数表
// ============================================================
void ActionEditDialog::rebuildParamGrid() {
    m_grid->Clear();

    const bool withButton = hasButton(m_action.type);
    const bool withCoords = hasCoordinates(m_action.type);

    // 顺序固定：按键 → 坐标 → 滚轮 → 键 → 延时 → 按住。
    // 各类型原有的字段相对顺序都能被这个序列表达，切换类型时视觉位置不跳。
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
// 从网格读回字段（只处理当前类型存在的属性）
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
        // hotkey 由"设置按键"按钮单独更新，网格里只是显示
    }

    m_action.delayMs = get(kPropDelay);

    if (m_action.type == ActionType::MouseClick) {
        m_action.holdMs = get(kPropHold);
    }
}

// ============================================================
// 辅助按钮启用
// ============================================================
// ReSharper disable once CppMemberFunctionMayBeConst
void ActionEditDialog::refreshAuxButtons() {
    const bool withCoords = hasCoordinates(m_action.type);
    const bool absolute = withCoords && !m_action.relative;

    m_chkRelative->Enable(withCoords);
    m_chkRelative->SetValue(m_action.relative);

    // 捕捉坐标只对绝对坐标有意义（相对动作要的是偏移量，不是屏幕上的点）
    m_btnCapturePos->Enable(absolute);
    m_btnSetKey->Enable(m_action.type == ActionType::Keyboard);
}

// ============================================================
// 捕捉坐标：覆盖层 → 回填 x/y + 更新网格
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
// 设置按键：热键捕获 → 更新 hotkey + 网格显示
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
// 确定：先从网格收集，再让对话框正常关闭
// ============================================================
void ActionEditDialog::onOk(wxCommandEvent &event) {
    collectFromGrid();
    LOG_DEBUG("action dialog: ok, type={}", static_cast<int>(m_action.type));
    event.Skip();
}
