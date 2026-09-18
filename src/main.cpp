#include <wx/wx.h>

#include "common/Log.hpp"
#include "managers/LanguageManager.hpp"
#include "ui/MainFrame.hpp"

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
