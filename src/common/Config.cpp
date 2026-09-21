#include <algorithm>
#include <utility>

#include "common/Config.hpp"
#include "common/Types.hpp"
#include "common/Hotkey.hpp"


// ============================================================
// 序列化
//
// to_json 负责写入 version 字段；from_json 不读它——
// 版本校验在 ConfigPaths::loadFrom 里做，Config 结构本身
// 只描述"当前版本"的字段。
// ============================================================

void to_json(nlohmann::json &j, const Config &c) {
    j = nlohmann::json::object();

    j["version"] = kConfigVersion;
    j["language"] = c.language;
    j["timerMode"] = timerModeToString(c.timerMode);
    j["alwaysOnTop"] = c.alwaysOnTop;
    j["tabIndex"] = c.tabIndex;

    auto hotkeys = nlohmann::json::array();
    for (const auto &hk: c.hotkeys) {
        hotkeys.push_back(hk);
    }
    j["hotkeys"] = std::move(hotkeys);
    j["hotkeyEnabled"] = c.hotkeyEnabled;

    j["window"] = nlohmann::json{
        {"x", c.window.x},
        {"y", c.window.y},
        {"width", c.window.width},
        {"height", c.window.height},
        {"maximized", c.window.maximized},
    };

    j["flags"] = nlohmann::json{
        {"spinWarningConfirmed", c.flags.spinWarningConfirmed},
    };
}

void from_json(const nlohmann::json &j, Config &c) {
    c = Config{};

    c.language = j.value("language", std::string{});
    c.timerMode = timerModeFromString(
        j.value("timerMode", std::string{"sleep"}));
    c.alwaysOnTop = j.value("alwaysOnTop", false);
    c.tabIndex = j.value("tabIndex", 0);

    // hotkeys：长度可能多于/少于 kHotkeyCount，只取前 kHotkeyCount 项；
    // 缺失的保持默认（全 0 空热键）。
    if (j.contains("hotkeys") && j["hotkeys"].is_array()) {
        const auto &arr = j["hotkeys"];
        const auto n = std::min<std::size_t>(arr.size(),
                                             static_cast<std::size_t>(kHotkeyCount));
        for (std::size_t i = 0; i < n; ++i) {
            if (arr[i].is_object()) {
                c.hotkeys[i] = arr[i].get<Hotkey>();
            }
        }
    }

    // hotkeyEnabled：长度可能多于/少于 kHotkeyCount，只取前 kHotkeyCount 项；
    // 缺失的部分保持 kHotkeyEnabledDefault（全 true），兼容老配置。
    if (j.contains("hotkeyEnabled") && j["hotkeyEnabled"].is_array()) {
        const auto &arr = j["hotkeyEnabled"];
        const auto n = std::min<std::size_t>(arr.size(),
                                             static_cast<std::size_t>(kHotkeyCount));
        for (std::size_t i = 0; i < n; ++i) {
            if (arr[i].is_boolean()) {
                c.hotkeyEnabled[i] = arr[i].get<bool>();
            }
        }
    }

    // window：缺失字段走默认值（旧配置兼容）
    if (j.contains("window") && j["window"].is_object()) {
        const auto &w = j["window"];
        c.window.x = w.value("x", -1);
        c.window.y = w.value("y", -1);
        c.window.width = w.value("width", 550);
        c.window.height = w.value("height", 710);
        c.window.maximized = w.value("maximized", false);
    }

    if (j.contains("flags") && j["flags"].is_object()) {
        const auto &f = j["flags"];
        c.flags.spinWarningConfirmed = f.value("spinWarningConfirmed", false);
    }
}
