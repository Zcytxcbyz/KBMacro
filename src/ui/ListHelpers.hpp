#pragma once

#include <algorithm>

#include <wx/listctrl.h>
#include <wx/scrolwin.h>

// 列表相关的两个小工具，只服务"装进滚动页的短列表"。

namespace uifit {

    // 把列表撑到刚好放下全部行，让列表内部不出滚动条（滚动交给页面）。
    // 用 GetItemRect 取最后一行的底边，跟系统 DPI、主题行高都无关。
    inline void fitListToRows(wxListCtrl *list) {
        if (list == nullptr || list->GetItemCount() == 0) {
            return;
        }

        wxRect lastRow;
        if (!list->GetItemRect(list->GetItemCount() - 1, lastRow)) {
            return;
        }

        list->SetMinSize(wxSize(wxDefaultCoord, lastRow.GetBottom() + 6));

        // SetMinSize 只改约束，必须让父窗口重新布局才会真的变高
        if (wxWindow *parent = list->GetParent()) {
            parent->Layout();
        }
    }

    // 让子控件的滚轮事件转给外层的 wxScrolledWindow。
    // 否则鼠标停在列表上时，滚轮被列表自己吃掉，页面纹丝不动。
    inline void forwardWheelToPage(wxWindow *child) {
        if (child == nullptr) {
            return;
        }

        child->Bind(wxEVT_MOUSEWHEEL, [child](wxMouseEvent &event) {
            for (wxWindow *w = child->GetParent(); w != nullptr; w = w->GetParent()) {
                auto *page = wxDynamicCast(w, wxScrolledWindow);
                if (page == nullptr) {
                    continue;
                }

                int x = 0;
                int y = 0;
                page->GetViewStart(&x, &y);

                // 一个滚轮档滚 3 行；高分辨率滚轮 rotation 是 120 的倍数
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
