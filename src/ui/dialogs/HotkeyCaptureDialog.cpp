#include "ui/dialogs/HotkeyCaptureDialog.hpp"

// ReSharper disable once CppUnusedIncludeDirective
#include <wx/statbox.h>
#include <utility>

#include "ui/DialogButtons.hpp"
#include "common/Hotkey.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"   // ToUtf8
#include "managers/LanguageManager.hpp"

HotkeyCaptureDialog::HotkeyCaptureDialog(wxWindow *parent,
                                         const wxString &title,
                                         const Hotkey &initial,
                                         const bool requireMainKey,
                                         ConflictChecker checkConflict)
    : wxDialog(parent, wxID_ANY, title),
      m_hotkey(initial),
      m_requireMainKey(requireMainKey),
      m_checkConflict(std::move(checkConflict)) {
    auto *root = new wxBoxSizer(wxVERTICAL);

    // ---- 捕获显示区 ----
    m_display = new wxTextCtrl(this, wxID_ANY, "",
                               wxDefaultPosition, FromDIP(wxSize(360, 32)),
                               wxTE_READONLY | wxTE_CENTRE);
    root->Add(m_display, 0, wxEXPAND | wxALL, 12);

    // ---- 按钮行：清除 ----
    auto *clearRow = new wxBoxSizer(wxHORIZONTAL);
    auto *btnClear = new wxButton(this, wxID_ANY, TR(IDS_HOTKEY_DIALOG_BTN_CLEAR));
    clearRow->Add(btnClear, 0, 0);
    clearRow->AddStretchSpacer(1);
    root->Add(clearRow, 0, wxEXPAND | wxLEFT | wxRIGHT, 12);

    // ---- 标准按钮行：确定/取消 ----
    auto *btnSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    root->Add(btnSizer, 0, wxEXPAND | wxALL, 12);

    uibtn::localize(this);   // 标准按钮文案换成本地化文案（wx 不带语言文件）

    SetSizerAndFit(root);
    wxTopLevelWindowBase::SetMinSize(wxSize(FromDIP(400), -1));

    // ---- 焦点 & 键盘捕获 ----
    // wxEVT_CHAR_HOOK 在键盘输入路由到子控件前触发，用于拦截所有按键
    Bind(wxEVT_CHAR_HOOK, &HotkeyCaptureDialog::onCharHook, this);
    btnClear->Bind(wxEVT_BUTTON, &HotkeyCaptureDialog::onClear, this);
    Bind(wxEVT_BUTTON, &HotkeyCaptureDialog::onOk, this, wxID_OK);

    updateDisplay();
    m_display->SetFocus();
}

// ============================================================
// 键盘捕获
// ============================================================
void HotkeyCaptureDialog::onCharHook(const wxKeyEvent &event) {
    const unsigned rawVk = event.GetRawKeyCode();

    // Esc：清空当前捕获，不关闭对话框
    if (rawVk == VK_ESCAPE) {
        m_hotkey = Hotkey{};
        updateDisplay();
        return;
    }

    // 合并当前按下的修饰键（只累加，不清除）
    const int wxMods = event.GetModifiers();
    if (wxMods & wxMOD_CONTROL) m_hotkey.modifiers |= hk::kModCtrl;
    if (wxMods & wxMOD_SHIFT)   m_hotkey.modifiers |= hk::kModShift;
    if (wxMods & wxMOD_ALT)     m_hotkey.modifiers |= hk::kModAlt;
    if (wxMods & wxMOD_WIN)     m_hotkey.modifiers |= hk::kModWin;

    // 主键（非修饰键）：更新 vk
    if (!utils::isModifierKey(rawVk)) {
        m_hotkey.vk = rawVk;
    }

    LOG_DEBUG("hotkey dialog: captured '{}'", utils::ToUtf8(utils::hotkeyToString(m_hotkey)));
    updateDisplay();
    // 不调用 event.Skip()，阻止按键传播到子控件
}

// ============================================================
// 清除 / 确定
// ============================================================
void HotkeyCaptureDialog::onClear(wxCommandEvent &) {
    m_hotkey = Hotkey{};
    updateDisplay();
    m_display->SetFocus();
}

void HotkeyCaptureDialog::onOk(wxCommandEvent &event) {
    if (!utils::isValidHotkey(m_hotkey, m_requireMainKey)) {
        uibtn::messageBox(TR(IDS_HOTKEY_DIALOG_MSG_INVALID),
                     GetTitle(),
                     wxOK | wxICON_WARNING, this);
        return;
    }

    // 冲突检查：空热键（清除）不参与
    const bool isEmpty = (m_hotkey.vk == 0 && m_hotkey.modifiers == 0);
    if (!isEmpty && m_checkConflict && m_checkConflict(m_hotkey)) {
        uibtn::messageBox(TR(IDS_HOTKEY_DIALOG_MSG_CONFLICT),
                     GetTitle(),
                     wxOK | wxICON_WARNING, this);
        return;
    }

    event.Skip();
}

// ============================================================
// 显示刷新
// ============================================================
void HotkeyCaptureDialog::updateDisplay() const {
    if (const std::wstring s = utils::hotkeyToString(m_hotkey); s.empty()) {
        m_display->SetValue(TR(IDS_HOTKEY_DIALOG_HINT));
    } else {
        m_display->SetValue(s);
    }
}
