#pragma once

#include <functional>

#include <wx/wx.h>

#include "common/Types.hpp"

// 热键捕获对话框。
//
// 用法：
//     HotkeyCaptureDialog dlg(this, "设置按键", currentHotkey, false);
//     if (dlg.ShowModal() == wxID_OK) {
//         Hotkey result = dlg.getHotkey();
//     }
//
// requireMainKey == true  必须有主键（全局热键用）
// requireMainKey == false 允许纯修饰键或纯主键（键盘模拟用）
//
// checkConflict（可选）：外部冲突检测函数。非空时，onOk 会调用它，
//   返回 true 表示该热键与已有热键冲突，此时弹出提示且不关闭对话框。
//   用于 SettingsPanel 修改全局热键时的"与其他热键重复"检查。
class HotkeyCaptureDialog : public wxDialog {
public:
    using ConflictChecker = std::function<bool(const Hotkey &)>;

    HotkeyCaptureDialog(wxWindow *parent,
                        const wxString &title,
                        const Hotkey &initial,
                        bool requireMainKey,
                        ConflictChecker checkConflict = {});

    [[nodiscard]] Hotkey getHotkey() const noexcept { return m_hotkey; }

private:
    void onCharHook(const wxKeyEvent &event);
    void onClear(wxCommandEvent &event);
    void onOk(wxCommandEvent &event);

    void updateDisplay() const;

    Hotkey m_hotkey;
    bool m_requireMainKey = false;
    ConflictChecker m_checkConflict;   // 空 = 不检查冲突
    wxTextCtrl *m_display{};
};
