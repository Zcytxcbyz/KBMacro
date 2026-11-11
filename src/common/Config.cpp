#include <algorithm>
#include <cmath>
#include <utility>

#include "common/Config.hpp"
#include "common/Hotkey.hpp"
#include "common/Types.hpp"

// ============================================================
// serialization
//
// to_json writes the version field; from_json does not read it:
// version validation happens in ConfigPaths::loadFrom, the Config struct itself
// only describes the fields of "the current version".
// ============================================================

void to_json(nlohmann::json &j, const Config &c) {
    j = nlohmann::json::object();

    j["version"] = kConfigVersion;
    j["language"] = c.language;
    j["timerMode"] = timerModeToString(c.timerMode);
    j["timerHybridSpinMs"] = c.timerHybridSpinMs;
    j["recordMoveIntervalMs"] = c.recordMoveIntervalMs;
    j["recordIgnoreInjected"] = c.recordIgnoreInjected;
    j["recordStartDelayMs"] = c.recordStartDelayMs;
    j["recordMaxActions"] = c.recordMaxActions;
    j["playbackSpeed"] = c.playbackSpeed;
    j["alwaysOnTop"] = c.alwaysOnTop;
    j["tabIndex"] = c.tabIndex;

    auto hotkeys = nlohmann::json::array();
    for (const auto &hk : c.hotkeys) {
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
            {"adaptationPreference", adaptationPreferenceToString(c.flags.adaptationPreference)},
    };
}

void from_json(const nlohmann::json &j, Config &c) {
    c = Config{};

    c.language = j.value("language", std::string{});
    c.timerMode = timerModeFromString(j.value("timerMode", std::string{"sleep"}));
    // hand-edited config may hold out-of-range values, clamp once on load; missing fields default (old configs)
    c.timerHybridSpinMs = std::clamp(
            j.value("timerHybridSpinMs", kTimerHybridSpinMsDefault), kTimerHybridSpinMsMin, kTimerHybridSpinMsMax);
    c.recordMoveIntervalMs = std::clamp(j.value("recordMoveIntervalMs", kRecordMoveIntervalMsDefault),
                                        kRecordMoveIntervalMsMin,
                                        kRecordMoveIntervalMsMax);
    c.recordIgnoreInjected = j.value("recordIgnoreInjected", false);
    c.recordStartDelayMs = std::clamp(
            j.value("recordStartDelayMs", kRecordStartDelayMsDefault), kRecordStartDelayMsMin, kRecordStartDelayMsMax);
    c.recordMaxActions = std::clamp(
            j.value("recordMaxActions", kRecordMaxActionsDefault), kRecordMaxActionsMin, kRecordMaxActionsMax);
    c.playbackSpeed = snapPlaybackSpeed(j.value("playbackSpeed", kPlaybackSpeedDefault));
    c.alwaysOnTop = j.value("alwaysOnTop", false);
    c.tabIndex = j.value("tabIndex", 0);

    // hotkeys: length may be more/less than kHotkeyCount, only the first kHotkeyCount are taken;
    // missing entries keep the default (all-zero empty hotkeys).
    if (j.contains("hotkeys") && j["hotkeys"].is_array()) {
        const auto &arr = j["hotkeys"];
        const auto n = std::min<std::size_t>(arr.size(), static_cast<std::size_t>(kHotkeyCount));
        for (std::size_t i = 0; i < n; ++i) {
            if (arr[i].is_object()) {
                c.hotkeys[i] = arr[i].get<Hotkey>();
            }
        }
    }

    // hotkeyEnabled: length may be more/less than kHotkeyCount, only the first kHotkeyCount are taken;
    // missing entries keep kHotkeyEnabledDefault (all true), old configs stay compatible.
    if (j.contains("hotkeyEnabled") && j["hotkeyEnabled"].is_array()) {
        const auto &arr = j["hotkeyEnabled"];
        const auto n = std::min<std::size_t>(arr.size(), static_cast<std::size_t>(kHotkeyCount));
        for (std::size_t i = 0; i < n; ++i) {
            if (arr[i].is_boolean()) {
                c.hotkeyEnabled[i] = arr[i].get<bool>();
            }
        }
    }

    // window: missing fields use defaults (old config compat)
    if (j.contains("window") && j["window"].is_object()) {
        const auto &w = j["window"];
        c.window.x = w.value("x", -1);
        c.window.y = w.value("y", -1);
        c.window.width = w.value("width", 650);
        c.window.height = w.value("height", 720);
        c.window.maximized = w.value("maximized", false);
    }

    if (j.contains("flags") && j["flags"].is_object()) {
        const auto &f = j["flags"];
        c.flags.spinWarningConfirmed = f.value("spinWarningConfirmed", false);
        c.flags.adaptationPreference =
                adaptationPreferenceFromString(f.value("adaptationPreference", std::string{"Ask"}));
    }
}

double snapPlaybackSpeed(const double speed) noexcept {
    // NaN and <= 0 fall back to the default step (!(x > 0) also filters out NaN)
    if (!(speed > 0.0)) {
        return kPlaybackSpeedDefault;
    }

    double best = kPlaybackSpeeds[0];
    double bestDelta = std::abs(speed - best);
    for (const double candidate : kPlaybackSpeeds) {
        if (const double delta = std::abs(speed - candidate); delta < bestDelta) {
            best = candidate;
            bestDelta = delta;
        }
    }
    return best;
}
