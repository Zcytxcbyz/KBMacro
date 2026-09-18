#pragma once

#include <wx/wx.h>
#include <wx/listctrl.h>

// 录制与编排面板。
//
// 当前为纯 UI 阶段：控件已布局，按钮事件只记录日志，不接实际逻辑。
// 动作列表里填了几行临时假数据用于预览列宽，接逻辑时清掉。
// 编辑对话框、坐标适配弹窗、ThreadManager 对接等后续补齐。
class RecorderPanel : public wxPanel {
public:
    explicit RecorderPanel(wxWindow *parent);

private:
    void buildUi();

    void onStartRecord(wxCommandEvent &event);

    void onStopRecord(wxCommandEvent &event);

    void onEdit(wxCommandEvent &event);

    void onDelete(wxCommandEvent &event);

    void onMoveUp(wxCommandEvent &event);

    void onMoveDown(wxCommandEvent &event);

    void onClear(wxCommandEvent &event);

    void onStartPlayback(wxCommandEvent &event);

    void onStopPlayback(wxCommandEvent &event);

    void onImportScript(wxCommandEvent &event);

    void onExportScript(wxCommandEvent &event);

    // ---- 录制 ----
    wxButton *m_btnStartRecord{};
    wxButton *m_btnStopRecord{};

    // ---- 动作列表 ----
    wxListCtrl *m_listCtrl{};
    wxButton *m_btnEdit{};
    wxButton *m_btnDelete{};
    wxButton *m_btnMoveUp{};
    wxButton *m_btnMoveDown{};
    wxButton *m_btnClear{};

    // ---- 运行模式 ----
    wxChoice *m_choiceRunMode{};
    wxSpinCtrl *m_spinDuration{};
    wxSpinCtrl *m_spinCount{};

    // ---- 回放 ----
    wxButton *m_btnStartPlayback{};
    wxButton *m_btnStopPlayback{};
    wxButton *m_btnImportScript{};
    wxButton *m_btnExportScript{};
};
