#include <wx/notebook.h>

#include "managers/LanguageManager.hpp"

#include "ui/MainFrame.hpp"
#include "ui/KBMacroStatusBar.hpp"

#include "ui/panels/ClickerPanel.hpp"
#include "ui/panels/KeyboardPanel.hpp"
#include "ui/panels/RecorderPanel.hpp"
#include "ui/panels/SettingsPanel.hpp"
#include "ui/panels/AboutPanel.hpp"

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, TR(IDS_APP_TITLE),
              wxDefaultPosition, wxSize(550, 710)) {
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
    auto *settingsPanel = new SettingsPanel(notebook);
    notebook->AddPage(settingsPanel, TR(IDS_TAB_SETTINGS));

    // ---- 标签页5：关于 ----
    auto *aboutPanel = new AboutPanel(notebook);
    notebook->AddPage(aboutPanel, TR(IDS_TAB_ABOUT));

    wxWindowBase::SetBackgroundColour(wxColour(240, 240, 240));

    wxFrameBase::SetStatusBar(new KBMacroStatusBar(this));
}
