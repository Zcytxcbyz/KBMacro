#pragma once

#include <cstdint>
#include <vector>

#include "common/Action.hpp"

// 录制事件的归一化：原始事件 → Action 序列。
//
// 刻意不依赖 <windows.h>：整个归一化是纯函数，单测不需要任何 Win32 环境，
// 也不读配置、不打日志。钩子那边怎么填事件是另一个模块的事。

namespace recorder {

    // 钩子回调交给归一化器的原始事件类型
    enum class RawKind {
        KeyDown,
        KeyUp,
        Move,
        ButtonDown,
        ButtonUp,
        Wheel,
    };

    struct RawEvent {
        RawKind kind = RawKind::Move;

        int x = 0;                 // 鼠标事件的位置（键盘事件不用）
        int y = 0;
        MouseButton button = MouseButton::Left;   // ButtonDown / ButtonUp

        unsigned vk = 0;           // KeyDown / KeyUp
        int wheelDelta = 0;        // Wheel：WHEEL_DELTA 的分数，正 = 上/右
        std::uint32_t time = 0;    // GetTickCount 毫秒，会回绕（一律按无符号差值算）
        bool horizontal = false;   // Wheel：横向（MOUSEEVENTF_HWHEEL）
    };

    // 归一化要用的策略位，与 Recorder 的 RecordOptions 一一对应
    struct NormalizeOptions {
        unsigned startKeyVk = 0;   // 开始热键主键（0 = 未配置）
        unsigned stopKeyVk = 0;    // 停止热键主键
        unsigned startMods = 0;    // 开始热键修饰键掩码
        unsigned stopMods = 0;     // 停止热键修饰键掩码
        int maxActions = 100000;   // 0 = 不限
    };

    // 头尾剔除、主键过滤、自动重复合并、点击/拖动判定、滚轮累积、delayMs 计算、
    // 动作上限 —— 全部在这里完成。产物为空表示"没录到东西"。
    [[nodiscard]] std::vector<Action> normalize(const std::vector<RawEvent> &events,
                                                const NormalizeOptions &opts);

} // namespace recorder
