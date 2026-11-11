#pragma once

#include <functional>

#include <wx/wx.h>

// Full-screen coordinate capture window.
//
// usage:
//     auto *overlay = new ScreenCaptureOverlay(this,
//         [this](bool captured, wxPoint pos) {
//             if (captured) {
//                 m_x->SetValue(wxString::Format("%d", pos.x));
//                 m_y->SetValue(wxString::Format("%d", pos.y));
//             }
//         });
//
// Shows itself on construction and destroys itself after interaction; caller keeps no pointer.
//
// interaction:
//   - left click: callback (true, virtual desktop coordinate)
//   - Esc / right click: callback (false, (0,0))
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
    wxPoint m_origin;   // top-left of the virtual desktop (screen coordinates)
};
