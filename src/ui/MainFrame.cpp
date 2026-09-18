#include <wx/notebook.h>

#include "managers/LanguageManager.hpp"

#include "ui/MainFrame.hpp"
#include "ui/KBMacroStatusBar.hpp"

#include "ui/panels/ClickerPanel.hpp"
#include "ui/panels/KeyboardPanel.hpp"
#include "ui/panels/RecorderPanel.hpp"

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, TR(IDS_APP_TITLE),
              wxDefaultPosition, wxSize(550, 700)) {
    auto *notebook = new wxNotebook(this, wxID_ANY);

    // ---- 标签页1：鼠标连点器 ----
    auto *clickerPanel = new ClickerPanel(notebook);
    notebook->AddPage(clickerPanel, TR(IDS_TAB_CLICKER));

    // ---- 标签页2：键盘模拟器 ----
    auto *keyboardPanel = new KeyboardPanel(notebook);
    notebook->AddPage(keyboardPanel, TR(IDS_TAB_KEYBOARD));

    // ---- 标签页3：录制与编排 ----
    auto *recorderPanel = new RecorderPanel(notebook);
    notebook->AddPage(recorderPanel, TR(IDS_TAB_RECORDER));

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
