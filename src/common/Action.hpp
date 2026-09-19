#pragma once

#include "common/Types.hpp"

// ============================================================
// 录制动作
// ============================================================

// 动作类型
enum class ActionType {
    MouseMove,
    MouseClick,
    MouseWheel,
    Keyboard,
};

// 鼠标按键
enum class MouseButton {
    Left,
    Right,
    Middle,
    Side1,
    Side2,
};

// 滚轮方向
enum class WheelDirection {
    Up,
    Down,
    Left,
    Right,
};

// 键盘事件类型
enum class KeyEventType {
    Down,
    Up,
};

// 一个录制动作。
//
// 值类型：可拷贝、可移动。字段固定，由 type 决定哪些字段有意义：
//   MouseMove : x, y, delayMs
//   MouseClick: button, x, y, holdMs, delayMs
//   MouseWheel: wheelDirection, wheelSteps, delayMs
//   Keyboard  : hotkey, keyEvent, delayMs
struct Action {
    ActionType type = ActionType::MouseMove;

    // MouseMove / MouseClick
    int x = 0;
    int y = 0;

    // MouseClick
    MouseButton button = MouseButton::Left;
    int holdMs = 0;

    // MouseWheel
    WheelDirection wheelDirection = WheelDirection::Up;
    int wheelSteps = 1;

    // Keyboard
    Hotkey hotkey;
    KeyEventType keyEvent = KeyEventType::Down;

    // 所有类型都有：距上一个动作的间隔
    int delayMs = 0;
};
