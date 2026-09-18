#pragma once

#include <wx/wx.h>

// 主窗口。
//
// 职责：
//   - 创建并持有 wxNotebook
//   - 组装五个标签页
//   - 处理窗口级样式、尺寸
//
// 各标签页内部的具体控件与逻辑，由 ui/panels/ 下的面板类实现。
class MainFrame : public wxFrame {
public:
    MainFrame();
};
