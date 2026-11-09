#pragma once

#include <vector>

#include <wx/wx.h>
#include <wx/listctrl.h>

#include "common/Action.hpp"
#include "common/Types.hpp"

#include "managers/ThreadManager.hpp"

// 录制与编排面板。
//
// 当前为过渡期：面板自己持有 std::vector<Action> 作为数据源，
// 将来 Recorder 模块做出来后，数据挪到模块里，本面板通过接口读写。
//
// 本阶段：
//   - 提供公开方法（供热键路径调用）
//   - 支持宏脚本的导入 / 导出（*.macro.json，整体替换语义）
class RecorderPanel : public wxPanel {
public:
    RecorderPanel(wxWindow *parent, ThreadManager &threadMgr);

    // 供 MainFrame 从热键路径调用，与用户点击对应按钮等价。
    // 录制与回放互斥，由 ThreadManager 内部保证；面板只管发起请求。
    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();

private:
    void buildUi();

    // 从控件组装 RunSpec（回放用）
    [[nodiscard]] RunSpec collectRunSpec() const;

    // 把 RunSpec 同步到控件（导入脚本用）
    void applyRunSpec(const RunSpec &run);

    // 屏幕规格不一致时询问用户如何处理坐标
    enum class AdaptChoice {
        Adapt,
        KeepOriginal,
        Cancelled,
    };

    // 弹窗询问用户是否适配坐标。内部会先查 adaptationPreference 记忆。
    [[nodiscard]] AdaptChoice askAdaptCoordinates(
        const ScreenSpec &scriptSpec, const ScreenSpec &currentSpec);

    // ---- 录制 / 回放 / 导入导出 ----
    void onStartRecord(wxCommandEvent &event);
    void onStopRecord(wxCommandEvent &event);
    void onStartPlayback(wxCommandEvent &event);
    void onStopPlayback(wxCommandEvent &event);
    void onImportScript(wxCommandEvent &event);
    void onExportScript(wxCommandEvent &event);

    // ---- 动作列表操作 ----
    void onAdd(wxCommandEvent &event);
    void onEdit(wxCommandEvent &event);
    void onDelete(wxCommandEvent &event);
    void onMoveUp(wxCommandEvent &event);
    void onMoveDown(wxCommandEvent &event);
    void onClear(wxCommandEvent &event);

    void onListSelectionChanged(wxListEvent &event);

    // ---- 运行模式 ----
    void onRunModeChanged(wxCommandEvent &event);
    void refreshEnableStates();

    // ---- 列表维护 ----
    void refreshList();
    void refreshButtonStates();
    void selectRow(long row);
    [[nodiscard]] long getSelectedRow() const;

    // 切换回放按钮状态；同时按"列表是否为空"决定能不能开始
    void setRunning(bool running);

    // 切换录制按钮状态；录制期间同时锁住列表编辑、导入导出与回放
    void setRecording(bool recording);

    // 录制线程结束（停止或达到上限）后的交付：空表提示、非空走三选一弹窗
    void onRecordFinished(const TaskResult &res);
    void deliverRecording(const std::vector<Action> &actions);

    // ---- 数据 ----
    std::vector<Action> m_actions;
    bool m_recording = false;
    ThreadManager &m_threadMgr;

    // ---- 控件 ----
    wxButton *m_btnStartRecord{};
    wxButton *m_btnStopRecord{};

    wxListCtrl *m_listCtrl{};
    wxButton *m_btnAdd{};
    wxButton *m_btnEdit{};
    wxButton *m_btnDelete{};
    wxButton *m_btnMoveUp{};
    wxButton *m_btnMoveDown{};
    wxButton *m_btnClear{};

    wxChoice *m_choiceRunMode{};
    wxSpinCtrl *m_spinDuration{};
    wxSpinCtrl *m_spinCount{};
    wxSpinCtrl *m_spinStartDelay{};

    wxButton *m_btnStartPlayback{};
    wxButton *m_btnStopPlayback{};
    wxButton *m_btnImportScript{};
    wxButton *m_btnExportScript{};
};
