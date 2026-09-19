#include "common/KeyboardParams.hpp"

void to_json(nlohmann::json &j, const KeyboardParams &p) {
    j = nlohmann::json::object();
    j["hotkey"] = p.hotkey;
    j["intervalMs"] = p.intervalMs;
    j["holdMs"] = p.holdMs;
    j["run"] = p.run;
}

void from_json(const nlohmann::json &j, KeyboardParams &p) {
    p = KeyboardParams{};
    if (j.contains("hotkey") && j["hotkey"].is_object()) {
        p.hotkey = j["hotkey"].get<Hotkey>();
    }
    p.intervalMs = j.value("intervalMs", 100);
    p.holdMs = j.value("holdMs", 50);
    if (j.contains("run") && j["run"].is_object()) {
        p.run = j["run"].get<RunSpec>();
    }
}#include "common/KeyboardParams.hpp"

void to_json(nlohmann::json &j, const KeyboardParams &p) {
    j = nlohmann::json::object();
    j["hotkey"] = p.hotkey;
    j["intervalMs"] = p.intervalMs;
    j["holdMs"] = p.holdMs;
    j["run"] = p.run;
}

void from_json(const nlohmann::json &j, KeyboardParams &p) {
    p = KeyboardParams{};
    if (j.contains("hotkey") && j["hotkey"].is_object()) {
        p.hotkey = j["hotkey"].get<Hotkey>();
    }
    p.intervalMs = j.value("intervalMs", 100);
    p.holdMs = j.value("holdMs", 50);
    if (j.contains("run") && j["run"].is_object()) {
        p.run = j["run"].get<RunSpec>();
    }
}
