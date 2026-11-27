#include "common/Action.hpp"

#include <algorithm>

#include "common/Hotkey.hpp"
#include "common/Log.hpp"

namespace {

// Scripts are hand-editable JSON, so numbers can arrive negative, fractional or far outside what the
// field can hold. Converting straight into an int wraps around, which would replay a click at a wild
// coordinate or sleep for hours; clamp into range instead and fall back to the default for junk.
constexpr int kMaxCoord = 32767;
constexpr int kMaxDurationMs = 3600000; // one hour
constexpr int kMaxWheelSteps = 100;

int intField(const nlohmann::json &j, const char *key, const int lo, const int hi, const int def) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return def;

    // go through double: get<int64_t> throws on values that do not fit, double saturates to inf instead
    const double raw = it->get<double>();
    if (!(raw == raw)) return def; // NaN
    if (raw <= static_cast<double>(lo)) return lo;
    if (raw >= static_cast<double>(hi)) return hi;
    return static_cast<int>(raw);
}

} // namespace

void to_json(nlohmann::json &j, const Action &a) {
    j = nlohmann::json::object();
    j["type"] = actionTypeToString(a.type);

    // relative coords only matter for mouse actions; false is omitted, old scripts unaffected
    if (a.relative) {
        j["relative"] = true;
    }

    switch (a.type) {
        case ActionType::MouseMove:
            j["x"] = a.x;
            j["y"] = a.y;
            j["delayMs"] = a.delayMs;
            break;

        case ActionType::MouseClick:
            j["button"] = mouseButtonToString(a.button);
            j["x"] = a.x;
            j["y"] = a.y;
            j["holdMs"] = a.holdMs;
            j["delayMs"] = a.delayMs;
            break;

        case ActionType::MouseWheel:
            j["direction"] = wheelDirectionToString(a.wheelDirection);
            j["steps"] = a.wheelSteps;
            j["delayMs"] = a.delayMs;
            break;

        case ActionType::Keyboard:
            j["key"] = a.hotkey;
            j["event"] = keyEventTypeToString(a.keyEvent);
            j["delayMs"] = a.delayMs;
            break;
        case ActionType::MouseDown:
        case ActionType::MouseUp:
            j["button"] = mouseButtonToString(a.button);
            j["x"] = a.x;
            j["y"] = a.y;
            j["delayMs"] = a.delayMs;
            break;

        case ActionType::Delay:
            // here delayMs is the wait duration
            j["delayMs"] = a.delayMs;
            break;
    }
}

void from_json(const nlohmann::json &j, Action &a) {
    a = Action{};

    const std::string typeStr = j.value("type", std::string{"MouseMove"});
    const bool known =
            std::ranges::any_of(kActionTypeEntries, [&typeStr](const auto &entry) { return entry.name == typeStr; });
    if (!known) {
        // A newer version may write action types this build does not know. Falling back to MouseMove
        // would click at (0, 0) on the desktop, so keep it as a no-op delay instead.
        LOG_WARN("action: unknown type '{}', kept as a no-op", typeStr);
        a.type = ActionType::Delay;
        a.delayMs = 0;
        return;
    }

    a.type = actionTypeFromString(typeStr);
    a.delayMs = intField(j, "delayMs", 0, kMaxDurationMs, 0);
    a.relative = j.value("relative", false);

    switch (a.type) {
        case ActionType::MouseMove:
            a.x = intField(j, "x", -kMaxCoord, kMaxCoord, 0);
            a.y = intField(j, "y", -kMaxCoord, kMaxCoord, 0);
            break;

        case ActionType::MouseClick:
            a.button = mouseButtonFromString(j.value("button", std::string{"Left"}));
            a.x = intField(j, "x", -kMaxCoord, kMaxCoord, 0);
            a.y = intField(j, "y", -kMaxCoord, kMaxCoord, 0);
            a.holdMs = intField(j, "holdMs", 0, kMaxDurationMs, 0);
            break;

        case ActionType::MouseWheel:
            a.wheelDirection = wheelDirectionFromString(j.value("direction", std::string{"Up"}));
            a.wheelSteps = intField(j, "steps", 1, kMaxWheelSteps, 1);
            break;

        case ActionType::Keyboard:
            if (j.contains("key") && j["key"].is_object()) {
                a.hotkey = j["key"].get<Hotkey>();
            }
            a.keyEvent = keyEventTypeFromString(j.value("event", std::string{"Down"}));
            break;

        case ActionType::MouseDown:
        case ActionType::MouseUp:
            a.button = mouseButtonFromString(j.value("button", std::string{"Left"}));
            a.x = intField(j, "x", -kMaxCoord, kMaxCoord, 0);
            a.y = intField(j, "y", -kMaxCoord, kMaxCoord, 0);
            break;

        case ActionType::Delay:
            // delayMs was already read above
            break;
    }
}
