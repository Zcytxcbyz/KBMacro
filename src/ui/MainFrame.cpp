#include "ui/MainFrame.hpp"

#include <wx/notebook.h>

#include "KBMacroStatusBar.hpp"
#include "managers/LanguageManager.hpp"

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, TR(IDS_APP_TITLE),
              wxDefaultPosition, wxSize(500, 600)) {
    auto *notebook = new wxNotebook(this, wxID_ANY);

    // ---- 标签页1：鼠标连点器 ----
    // TODO: 替换为 ui/panels/ClickerPanel
    auto *page1 = new wxPanel(notebook, wxID_ANY);
    new wxStaticText(page1, wxID_ANY,
                     TR(IDS_CLICKER_PLACEHOLDER),
                     wxPoint(20, 20));
    notebook->AddPage(page1, TR(IDS_TAB_CLICKER));

    // ---- 标签页2：键盘模拟器 ----
    // TODO: 替换为 ui/panels/KeyboardPanel
    auto *page2 = new wxPanel(notebook, wxID_ANY);
    new wxStaticText(page2, wxID_ANY,
                     TR(IDS_KEYBOARD_PLACEHOLDER),
                     wxPoint(20, 20));
    notebook->AddPage(page2, TR(IDS_TAB_KEYBOARD));

    // ---- 标签页3：录制与编排 ----
    // TODO: 替换为 ui/panels/RecorderPanel
    auto *page3 = new wxPanel(notebook, wxID_ANY);
    new wxStaticText(page3, wxID_ANY,
                     TR(IDS_RECORDER_PLACEHOLDER),
                     wxPoint(20, 20));
    notebook->AddPage(page3, TR(IDS_TAB_RECORDER));

    // ---- 标签页4：设置 ----
    // TODO: 替换为 ui/panels/SettingsPanel
    auto *page4 = new wxPanel(notebook, wxID_ANY);
    new wxStaticText(page4, wxID_ANY,
                     TR(IDS_SETTINGS_PLACEHOLDER),
                     wxPoint(20, 20));
    notebook->AddPage(page4, TR(IDS_TAB_SETTINGS));

    // ---- 标签页5：关于 ----
    // TODO: 替换为 ui/panels/AboutPanel
    auto *page5 = new wxPanel(notebook, wxID_ANY);
    new wxStaticText(page5, wxID_ANY,
                     TR(IDS_ABOUT_PLACEHOLDER),
                     wxPoint(20, 20));
    notebook->AddPage(page5, TR(IDS_TAB_ABOUT));

    wxWindowBase::SetBackgroundColour(wxColour(240, 240, 240));

    wxFrameBase::SetStatusBar(new KBMacroStatusBar(this));
}
