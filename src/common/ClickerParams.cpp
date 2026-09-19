#include "common/ClickerParams.hpp"

void to_json(nlohmann::json &j, const ClickerParams &p) {
    j = nlohmann::json::object();
    j["ignorePosition"] = p.ignorePosition;
    j["x"] = p.x;
    j["y"] = p.y;
    j["clickType"] = clickTypeToString(p.clickType);
    j["wheelSteps"] = p.wheelSteps;
    j["intervalMs"] = p.intervalMs;
    j["holdMs"] = p.holdMs;
    j["run"] = p.run;
}

void from_json(const nlohmann::json &j, ClickerParams &p) {
    p = ClickerParams{};
    p.ignorePosition = j.value("ignorePosition", false);
    p.x = j.value("x", 0);
    p.y = j.value("y", 0);
    p.clickType = clickTypeFromString(j.value("clickType", std::string{"Left"}));
    p.wheelSteps = j.value("wheelSteps", 1);
    p.intervalMs = j.value("intervalMs", 100);
    p.holdMs = j.value("holdMs", 50);
    if (j.contains("run") && j["run"].is_object()) {
        p.run = j["run"].get<RunSpec>();
    }
}
