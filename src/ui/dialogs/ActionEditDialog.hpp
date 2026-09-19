#pragma once

#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>

#include "common/Action.hpp"

// 动作编辑对话框。
//
// 用法：
//     ActionEditDialog dlg(this, TR(IDS_ACTION_DIALOG_TITLE_ADD), Action{});
//     if (dlg.ShowModal() == wxID_OK) {
//         Action result = dlg.getAction();
//     }
//
// "添加"和"编辑"共用本对话框，标题由调用方决定。
//
// 交互：
//   - 顶部 wxChoice 选动作类型，切换时重建下方参数表
//   - 参数用 wxPropertyGrid 显示，可编辑
//   - 辅助按钮"捕捉坐标" / "设置按键"按当前动作类型启用
class ActionEditDialog : public wxDialog {
public:
    ActionEditDialog(wxWindow *parent, const wxString &title, const Action &action);

    [[nodiscard]] const Action &getAction() const noexcept { return m_action; }

private:
    void buildUi();

    void onTypeChanged(wxCommandEvent &event);

    void onCapturePosition(wxCommandEvent &event);

    void onSetKey(wxCommandEvent &event);

    void onOk(wxCommandEvent &event);

    // 按 m_action.type 重建参数表
    void rebuildParamGrid();

    // 从网格读回所有字段到 m_action
    void collectFromGrid();

    // 刷新辅助按钮的启用状态
    void refreshAuxButtons();

    Action m_action;
    wxChoice *m_choiceType{};
    wxPropertyGrid *m_grid{};
    wxButton *m_btnCapturePos{};
    wxButton *m_btnSetKey{};
};
