#include <wx/wx.h>

#include "common/Log.hpp"
#include "managers/ConfigManager.hpp"
#include "managers/LanguageManager.hpp"
#include "ui/MainFrame.hpp"

// ReSharper disable once CppUseInternalLinkage
class KBMacroApp : public wxApp {
public:
    bool OnInit() override {
        Log::Init();
        LOG_INFO("KBMacro starting");

        // ---- 1. load global config (decide path → read file) ----
        ConfigManager::instance().load();

        // ---- 2. decide the UI language ----
        //   config has a value   → use it (the user chose it)
        //   config is empty   → system language, not written back (the user never chose; don't decide for them)
        const std::string &code = ConfigManager::instance().config().language;
        if (!code.empty()) {
            if (LanguageManager::instance().setLanguageByCode(code)) {
                LOG_INFO("UI language from config: {}", code);
            } else {
                LOG_WARN("config language '{}' invalid, fallback to system", code);
                LanguageManager::instance().setLanguageFromSystem();
            }
        } else {
            LanguageManager::instance().setLanguageFromSystem();
            LOG_INFO("UI language from system (config empty)");
        }

        // ---- 3. main window ----
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
