#pragma once

#include <wx/statusbr.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <wx/string.h>

// 状态栏。
//
// 单一字段，显示当前状态文本。各面板运行时状态通过 SetMessage 更新。
// 字段划分、复杂状态管理接入 ThreadManager 时再补。
class KBMacroStatusBar : public wxStatusBar {
public:
    explicit KBMacroStatusBar(wxWindow *parent);

    // 显示状态文本（覆盖当前内容）
    void SetMessage(const wxString &text);

    // 右侧常驻提示（热键注册失败、配置异常之类）。
    // 与左边"运行状态"互不覆盖：状态每 300ms 重算，提示只在有事时写。
    void SetNotice(const wxString &text);

    // 清空状态文本
    void Clear();
};
