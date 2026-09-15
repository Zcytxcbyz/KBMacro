#include <wx/wx.h>
#include <wx/notebook.h>

#include "common/Log.hpp"
#include "managers/LanguageManager.hpp"

// ReSharper disable once CppUseInternalLinkage
class MainFrame : public wxFrame {
public:
    MainFrame() : wxFrame(nullptr, wxID_ANY, TR(IDS_APP_TITLE), wxDefaultPosition, wxSize(500, 600)) {
        auto *notebook = new wxNotebook(this, wxID_ANY);

        // ---- 标签页1：鼠标连点器 ----
        auto *page1 = new wxPanel(notebook, wxID_ANY);
        new wxStaticText(page1, wxID_ANY,
                         TR(IDS_CLICKER_PLACEHOLDER),
                         wxPoint(20, 20));
        notebook->AddPage(page1, TR(IDS_TAB_CLICKER));

        // ---- 标签页2：键盘模拟器 ----
        auto *page2 = new wxPanel(notebook, wxID_ANY);
        new wxStaticText(page2, wxID_ANY,
                         TR(IDS_KEYBOARD_PLACEHOLDER),
                         wxPoint(20, 20));
        notebook->AddPage(page2, TR(IDS_TAB_KEYBOARD));

        // ---- 标签页3：录制与编排 ----
        auto *page3 = new wxPanel(notebook, wxID_ANY);
        new wxStaticText(page3, wxID_ANY,
                         TR(IDS_RECORDER_PLACEHOLDER),
                         wxPoint(20, 20));
        notebook->AddPage(page3, TR(IDS_TAB_RECORDER));

        // ---- 标签页4：设置 ----
        auto *page4 = new wxPanel(notebook, wxID_ANY);
        new wxStaticText(page4, wxID_ANY,
                         TR(IDS_SETTINGS_PLACEHOLDER),
                         wxPoint(20, 20));
        notebook->AddPage(page4, TR(IDS_TAB_SETTINGS));

        // ---- 标签页5：关于 ----
        auto *page5 = new wxPanel(notebook, wxID_ANY);
        new wxStaticText(page5, wxID_ANY,
                         TR(IDS_ABOUT_PLACEHOLDER),
                         wxPoint(20, 20));
        notebook->AddPage(page5, TR(IDS_TAB_ABOUT));

        wxWindowBase::SetBackgroundColour(wxColour(240, 240, 240));
    }
};


// ReSharper disable once CppUseInternalLinkage
class KBMacroApp : public wxApp {
public:
    bool OnInit() override {
        Log::Init();
        LOG_INFO("KBMacro starting");

        LanguageManager::instance().setLanguageFromSystem();
        LOG_INFO("UI language initialized from system");

        auto *frame = new MainFrame();
        frame->Show(true);
        return true;
    }

    int OnExit() override {
        LOG_INFO("KBMacro exiting");
        Log::Shutdown();
        return wxApp::OnExit();
    }
};


wxIMPLEMENT_APP(KBMacroApp);
