#pragma once

#include <functional>

#include <wx/wx.h>

// 全屏坐标捕获窗口。
//
// 用法：
//     auto *overlay = new ScreenCaptureOverlay(this,
//         [this](bool captured, wxPoint pos) {
//             if (captured) {
//                 m_x->SetValue(wxString::Format("%d", pos.x));
//                 m_y->SetValue(wxString::Format("%d", pos.y));
//             }
//         });
//
// 构造即显示，用户交互后自动 Destroy，调用方不需要持有指针。
//
// 交互：
//   - 左键点击：回调 (true, 虚拟桌面坐标)
//   - Esc / 右键：回调 (false, (0,0))
class ScreenCaptureOverlay : public wxFrame {
public:
    using Callback = std::function<void(bool captured, wxPoint pos)>;

    ScreenCaptureOverlay(wxWindow *parent, Callback callback);

private:
    void onPaint(wxPaintEvent &event);
    void onLeftDown(wxMouseEvent &event);
    void onRightDown(wxMouseEvent &event);
    void onCharHook(wxKeyEvent &event);
    void onSetCursor(wxSetCursorEvent &event);

    void finish(bool captured, wxPoint pos);

    Callback m_callback;
    bool m_finished = false;
    wxPoint m_origin;   // 虚拟桌面左上角（屏幕坐标）
};
