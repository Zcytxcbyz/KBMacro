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

    auto hotkeys = nlohmann::json::array();
    for (const auto &hk: c.hotkeys) {
        hotkeys.push_back(hk);
    }
    j["hotkeys"] = std::move(hotkeys);

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

    if (j.contains("flags") && j["flags"].is_object()) {
        const auto &f = j["flags"];
        c.flags.spinWarningConfirmed = f.value("spinWarningConfirmed", false);
    }
}
