#pragma once

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
class HotkeyCaptureDialog : public wxDialog {
public:
    HotkeyCaptureDialog(wxWindow *parent,
                        const wxString &title,
                        const Hotkey &initial,
                        bool requireMainKey);

    [[nodiscard]] Hotkey getHotkey() const noexcept { return m_hotkey; }

private:
    void onCharHook(const wxKeyEvent &event);
    void onClear(wxCommandEvent &event);
    void onOk(wxCommandEvent &event);

    void updateDisplay() const;

    Hotkey m_hotkey;
    bool m_requireMainKey = false;
    wxTextCtrl *m_display{};
};
