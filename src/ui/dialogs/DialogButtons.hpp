#pragma once

#include <wx/button.h>
#include <wx/window.h>

#include "managers/LanguageManager.hpp"
#include "resource.h"

// ============================================================
// 标准按钮文案本地化
//
// wxID_OK / wxID_CANCEL / wxID_YES / wxID_NO 这些标准按钮，文案来自 wxWidgets
// 自带的 wxstd.mo 翻译文件。本项目是单 EXE 便携版、不带 wx 的语言文件，
// 所以这些按钮在中文界面下仍显示英文。这里在按钮建好之后按 id 换成资源文案。
//
// 用法：CreateStdDialogButtonSizer() / CreateButtonSizer() 之后、
//       SetSizerAndFit() / SetSizer() 之前调用 uibtn::localize(this)。
// ============================================================
namespace uibtn {

    inline void localize(wxWindow *owner) {
        constexpr struct {
            int id;
            int resId;
        } kMap[] = {
            {wxID_OK, IDS_COMMON_BTN_OK},
            {wxID_CANCEL, IDS_COMMON_BTN_CANCEL},
            {wxID_YES, IDS_COMMON_BTN_YES},
            {wxID_NO, IDS_COMMON_BTN_NO},
        };

        for (const auto &entry: kMap) {
            if (auto *btn = wxDynamicCast(owner->FindWindow(entry.id), wxButton)) {
                btn->SetLabel(TR(entry.resId));
            }
        }
    }

} // namespace uibtn
