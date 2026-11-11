#include "common/Action.hpp"

#include "common/Hotkey.hpp"

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
    a.type = actionTypeFromString(typeStr);
    a.delayMs = j.value("delayMs", 0);
    a.relative = j.value("relative", false);

    switch (a.type) {
        case ActionType::MouseMove:
            a.x = j.value("x", 0);
            a.y = j.value("y", 0);
            break;

        case ActionType::MouseClick:
            a.button = mouseButtonFromString(j.value("button", std::string{"Left"}));
            a.x = j.value("x", 0);
            a.y = j.value("y", 0);
            a.holdMs = j.value("holdMs", 0);
            break;

        case ActionType::MouseWheel:
            a.wheelDirection = wheelDirectionFromString(j.value("direction", std::string{"Up"}));
            a.wheelSteps = j.value("steps", 1);
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
            a.x = j.value("x", 0);
            a.y = j.value("y", 0);
            break;

        case ActionType::Delay:
            // delayMs was already read above, no extra field needed
            break;
    }
}
