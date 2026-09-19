#pragma once

#include <nlohmann/json.hpp>

#include "common/Types.hpp"

// 键盘模拟器参数
struct KeyboardParams {
    Hotkey hotkey;          // 要发送的按键组合
    int intervalMs = 100;   // 重复间隔
    int holdMs = 50;        // 按键保持时间
    RunSpec run;
};

void to_json(nlohmann::json &j, const KeyboardParams &p);
void from_json(const nlohmann::json &j, KeyboardParams &p);
