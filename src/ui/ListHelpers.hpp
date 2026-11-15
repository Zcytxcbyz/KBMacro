#pragma once

#include <algorithm>
#include <vector>

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

// Fit every column to its widest cell, then hand the overflow back to the
// widest column so no horizontal scrollbar appears.
//   gutter: extra pixels added to column 0 (checkbox gutter / icon column)
// Columns never shrink below the width they had when the helper was called,
// so AppendColumn() keeps acting as the per-column floor.
inline void autoSizeColumns(wxListCtrl *list, const int gutter = 0) {
    if (list == nullptr || list->GetColumnCount() == 0) {
        return;
    }

    // Autosizing scans every item; a very long recording would make that wasteful.
    if (list->GetItemCount() > 500) {
        return;
    }

    const int count = list->GetColumnCount();
    std::vector<int> floors;
    floors.reserve(static_cast<std::size_t>(count));
    for (int col = 0; col < count; ++col) {
        floors.push_back(list->GetColumnWidth(col));
    }

    for (int col = 0; col < count; ++col) {
        list->SetColumnWidth(col, wxLIST_AUTOSIZE_USEHEADER);
        const int content = list->GetColumnWidth(col) + (col == 0 ? gutter : 0);
        list->SetColumnWidth(col, std::max(content, floors[static_cast<std::size_t>(col)]));
    }

    const int client = list->GetClientSize().x;
    if (client <= 0) {
        return;
    }

    int total = 0;
    for (int col = 0; col < count; ++col) {
        total += list->GetColumnWidth(col);
    }

    // Take the excess back from whichever column is currently the widest.
    int overflow = total - client;
    while (overflow > 0) {
        int widest = 0;
        for (int col = 1; col < count; ++col) {
            if (list->GetColumnWidth(col) > list->GetColumnWidth(widest)) {
                widest = col;
            }
        }
        const int slack = list->GetColumnWidth(widest) - floors[static_cast<std::size_t>(widest)];
        if (slack <= 0) {
            break;
        }
        const int shrink = std::min(overflow, slack);
        list->SetColumnWidth(widest, list->GetColumnWidth(widest) - shrink);
        overflow -= shrink;
    }
}

} // namespace uifit
