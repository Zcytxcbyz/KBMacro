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

        const int height = lastRow.GetBottom() + list->FromDIP(6);
        if (height == list->GetMinSize().GetHeight()) {
            return;   // 高度没变就别再 Layout，避免刷新时界面抖动
        }

        list->SetMinSize(wxSize(wxDefaultCoord, height));

        wxWindow *parent = list->GetParent();
        if (parent == nullptr) {
            return;
        }
        parent->Layout();

        // 滚动页的虚拟尺寸要跟着更新，否则滚动范围还是旧的
        for (wxWindow *w = parent; w != nullptr; w = w->GetParent()) {
            if (auto *page = wxDynamicCast(w, wxScrolledWindow)) {
                page->FitInside();
                break;
            }
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
