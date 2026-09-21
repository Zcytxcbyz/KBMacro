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

        // ---- 1. 加载全局配置（判定路径 → 读文件）----
        ConfigManager::instance().load();

        // ---- 2. 决定 UI 语言 ----
        //   config 有值   → 用它（用户主动选过）
        //   config 为空   → 系统语言，不写回（用户没选过，不替他做主）
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

        // ---- 3. 主窗口 ----
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
