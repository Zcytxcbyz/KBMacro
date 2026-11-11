#pragma once

#include <nlohmann/json.hpp>

#include "common/Types.hpp"

// keyboard simulator parameters
struct KeyboardParams {
    Hotkey hotkey; // key combination to send
    int intervalMs = 100; // repeat interval
    int holdMs = 50; // key hold duration
    RunSpec run;
};

void to_json(nlohmann::json &j, const KeyboardParams &p);
void from_json(const nlohmann::json &j, KeyboardParams &p);
