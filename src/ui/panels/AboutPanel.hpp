#pragma once

#include <wx/wx.h>
#include <wx/hyperlink.h>
#include <wx/listctrl.h>

// 关于面板。
//
// 显示程序信息、第三方库列表、所有许可全文（含 KBMacro 自身）。
// 许可全文从 RC 的 RCDATA 资源加载，拼接到一个只读多行文本框。
class AboutPanel : public wxPanel {
public:
    explicit AboutPanel(wxWindow *parent);

private:
    void buildUi();

    wxListCtrl *m_libList{};
};
