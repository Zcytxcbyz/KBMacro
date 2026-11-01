#pragma once

#include <stop_token>

#include "common/Action.hpp"          // MouseButton / WheelDirection
#include "common/ClickerParams.hpp"   // ClickType / ClickerParams
#include "common/ITimer.hpp"

// ============================================================
// 鼠标连点器 —— 把一份 ClickerParams 跑成真实的输入事件
//
// 职责：
//   - 按 RunController 的节奏循环，每轮按 ClickType 发一次（双击是两次）输入
//   - 返回实际执行了多少轮，面板用它显示"已完成 N 次"
//
// 不做：
//   - 不管线程、不管状态、不产生停止请求（stop_token 由调用方给）
//   - 不碰 UI、不认识 ThreadManager、不认识面板
//   - 循环体不打日志（热路径）；每次 SendInput 由 win32/Input 自己记
//
// 错误契约：input::SendInputError 直接往外抛，由 ThreadManager 兜底记 Failed
// ============================================================

namespace clicker {

    // ClickType 展开成"发什么、发几次"。纯函数，可单独测。
    //   - 双击 = 两次完整点击（各等 holdMs），times = 2
    //   - 滚轮类型 times 恒为 1（一次事件带 steps × WHEEL_DELTA）
    struct ClickStep {
        bool wheel = false;                              // true：用 direction + wheelSteps
        int times = 1;                                   // 要发几次
        MouseButton button = MouseButton::Left;          // wheel == false 时有效
        WheelDirection direction = WheelDirection::Up;   // wheel == true 时有效
    };

    [[nodiscard]] ClickStep toClickStep(ClickType type) noexcept;

    // 连点主循环，返回实际执行轮数。
    //   - 被 stop 打断时立即返回，返回值是已完成的轮数
    //   - 每轮：先 moveTo（除非 ignorePosition）→ 再按 ClickType 发输入
    //   - 轮间等待 = max(0, intervalMs - holdMs)（间隔是整个周期，按住时间抵扣）；
    //     滚轮类型没有按住，等待 = intervalMs
    [[nodiscard]] int run(const ClickerParams &p, const ITimer &timer, const std::stop_token &st);

} // namespace clicker
