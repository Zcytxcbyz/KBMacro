#pragma once

#include <wx/statusbr.h>

// 状态栏。
//
// 当前仅作为占位：wxFrame 底部保留一条状态栏，
// 避免将来加状态区时挤动 notebook 布局。
//
// 字段划分、更新接口等接入 ThreadManager 时再补。
class KBMacroStatusBar : public wxStatusBar {
public:
    explicit KBMacroStatusBar(wxWindow *parent);
};
