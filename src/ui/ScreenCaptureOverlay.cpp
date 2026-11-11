#include "ui/ScreenCaptureOverlay.hpp"

#include <utility>

#include <wx/dcclient.h>

#include <windows.h>

#include "common/Log.hpp"
#include "managers/LanguageManager.hpp"

namespace {
// overall opacity: 0 = fully transparent, 255 = opaque.
// too faint hides the hint, too dark hides the screen. around 100 is the compromise.
constexpr int kOverlayAlpha = 100;

// padding of the hint text background box
constexpr int kHintPadding = 24;
} // namespace

ScreenCaptureOverlay::ScreenCaptureOverlay(wxWindow *parent, Callback callback)
    : wxFrame(parent,
              wxID_ANY,
              wxEmptyString,
              wxDefaultPosition,
              wxDefaultSize,
              wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP | wxBORDER_NONE),
      m_callback(std::move(callback)) {
    // cover the virtual desktop (full span of all monitors, may include negative coordinates)
    const int x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    m_origin = wxPoint(x, y);
    SetSize(x, y, w, h);
    LOG_DEBUG("overlay: covering virtual screen ({}, {}), {}x{}", x, y, w, h);

    Bind(wxEVT_PAINT, &ScreenCaptureOverlay::onPaint, this);
    Bind(wxEVT_LEFT_DOWN, &ScreenCaptureOverlay::onLeftDown, this);
    Bind(wxEVT_RIGHT_DOWN, &ScreenCaptureOverlay::onRightDown, this);
    Bind(wxEVT_CHAR_HOOK, &ScreenCaptureOverlay::onCharHook, this);
    Bind(wxEVT_SET_CURSOR, &ScreenCaptureOverlay::onSetCursor, this);

    wxTopLevelWindowMSW::Show();
    wxTopLevelWindowMSW::Raise();
    wxTopLevelWindowMSW::SetTransparent(kOverlayAlpha);
    wxWindow::SetFocus();
}

// ============================================================
// paint: translucent black backdrop (window alpha) + opaque hint box in the center
// ============================================================
void ScreenCaptureOverlay::onPaint(wxPaintEvent &) {
    wxPaintDC dc(this);
    const wxSize sz = GetClientSize();

    wxFont font = GetFont();
    font.SetPointSize(font.GetPointSize() + 12);
    font.MakeBold();
    dc.SetFont(font);

    const wxString hint = TR(IDS_CAPTURE_HINT);
    const wxSize textSize = dc.GetMultiLineTextExtent(hint);

    const wxRect box((sz.x - textSize.x) / 2 - kHintPadding,
                     (sz.y - textSize.y) / 2 - kHintPadding,
                     textSize.x + kHintPadding * 2,
                     textSize.y + kHintPadding * 2);

    // hint box background: solid black rectangle
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(box);

    // hint text: pure white, centered
    dc.SetTextForeground(*wxWHITE);
    dc.DrawLabel(hint, box, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
}

// ============================================================
// interaction
// ============================================================
// ReSharper disable once CppParameterMayBeConstPtrOrRef
void ScreenCaptureOverlay::onLeftDown(wxMouseEvent &event) {
    const wxPoint local = event.GetPosition();
    finish(true, m_origin + local);
}

void ScreenCaptureOverlay::onRightDown(wxMouseEvent &) {
    finish(false, wxPoint(0, 0));
}

void ScreenCaptureOverlay::onCharHook(wxKeyEvent &event) {
    if (event.GetKeyCode() == WXK_ESCAPE) {
        finish(false, wxPoint(0, 0));
        return;
    }
    event.Skip();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void ScreenCaptureOverlay::onSetCursor(wxSetCursorEvent &event) { // NOLINT(*-convert-member-functions-to-static)
    event.SetCursor(wxCursor(wxCURSOR_CROSS));
}

// ============================================================
// finish: callback + self-destroy
// ============================================================
void ScreenCaptureOverlay::finish(const bool captured, const wxPoint pos) {
    if (m_finished) return;
    m_finished = true;

    if (captured) {
        LOG_DEBUG("overlay: picked ({}, {})", pos.x, pos.y);
    } else {
        LOG_DEBUG("overlay: cancelled");
    }

    // move off-screen first to avoid flicker
    Show(false);

    if (m_callback) {
        m_callback(captured, pos);
    }

    Destroy();
}
