#pragma once

#include <algorithm>

#include <wx/listctrl.h>
#include <wx/scrolwin.h>

// Two small list helpers, only for "short lists inside scrolled pages".

namespace uifit {

// Grow the list to exactly fit all rows so it shows no inner scrollbar (the page scrolls).
// Use GetItemRect on the bottom edge of the last row; independent of system DPI and theme row height.
inline void fitListToRows(wxListCtrl *list) {
    if (list == nullptr || list->GetItemCount() == 0) {
        return;
    }

    wxRect lastRow;
    if (!list->GetItemRect(list->GetItemCount() - 1, lastRow)) {
        return;
    }

    const int height = lastRow.GetBottom() + list->FromDIP(6);
    if (height == list->GetMinSize().GetHeight()) {
        return; // skip Layout when the height is unchanged to avoid flicker on refresh
    }

    list->SetMinSize(wxSize(wxDefaultCoord, height));

    wxWindow *parent = list->GetParent();
    if (parent == nullptr) {
        return;
    }
    parent->Layout();

    // update the scrolled page's virtual size too, otherwise its scroll range stays stale
    for (wxWindow *w = parent; w != nullptr; w = w->GetParent()) {
        if (auto *page = wxDynamicCast(w, wxScrolledWindow)) { // NOLINT(*-pro-type-static-cast-downcast)
            page->FitInside();
            break;
        }
    }
}

// Forward wheel events from the child control to the enclosing wxScrolledWindow.
// Otherwise the list swallows the wheel while the pointer is over it and the page never moves.
inline void forwardWheelToPage(wxWindow *child) {
    if (child == nullptr) {
        return;
    }

    child->Bind(wxEVT_MOUSEWHEEL, [child](wxMouseEvent &event) {
        for (wxWindow *w = child->GetParent(); w != nullptr; w = w->GetParent()) {
            auto *page = wxDynamicCast(w, wxScrolledWindow); // NOLINT(*-pro-type-static-cast-downcast)
            if (page == nullptr) {
                continue;
            }

            int x = 0;
            int y = 0;
            page->GetViewStart(&x, &y);

            // one wheel detent scrolls 3 lines; high-resolution wheels report rotation as a multiple of 120
            const int delta = (event.GetWheelDelta() != 0) ? event.GetWheelDelta() : 120;
            const int lines = -event.GetWheelRotation() * 3 / delta;
            y = std::max(0, y + lines);

            page->Scroll(x, y);
            return;
        }

        event.Skip();
    });
}

} // namespace uifit
