#pragma once

#include <wx/button.h>
#include <wx/window.h>
#include <wx/msgdlg.h>

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

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
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

    // ---- 消息框文案本地化 ----
    //
    // wxMessageBox 的标准按钮也来自 wxstd.mo，中文界面下同样显示英文。
    // 这里用 wxMessageDialog 手工设一次标签，返回值和 wxMessageBox 一致
    // （wxID_OK / wxID_YES / wxID_CANCEL …）。
    //
    // 参数顺序特意与 wxMessageBox 一样（text, caption, style, parent），
    // 这样调用点只需把 wxMessageBox( 换成 uibtn::messageBox( 就行。
    [[nodiscard]] inline wxMessageDialogBase::ButtonLabel label(const int resId) {
        return { wxMessageDialogBase::ButtonLabel(wxString(TR(resId))) };
    }

    inline int messageBox(const wxString &text, const wxString &caption,
                                        const long style = wxOK | wxICON_INFORMATION,
                                        wxWindow *parent = nullptr) {
        wxMessageDialog dlg(parent, text, caption, style);

        if ((style & wxYES_NO) != 0) {
            if ((style & wxCANCEL) != 0) {
                dlg.SetYesNoCancelLabels(label(IDS_COMMON_BTN_YES), label(IDS_COMMON_BTN_NO),
                                         label(IDS_COMMON_BTN_CANCEL));
            } else {
                dlg.SetYesNoLabels(label(IDS_COMMON_BTN_YES), label(IDS_COMMON_BTN_NO));
            }
        } else {
            dlg.SetOKLabel(label(IDS_COMMON_BTN_OK));
        }

        return dlg.ShowModal();
    }

} // namespace uibtn
