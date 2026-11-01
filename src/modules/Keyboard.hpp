#pragma once

#include <stop_token>

#include "common/ITimer.hpp"
#include "common/KeyboardParams.hpp"

// ============================================================
// 键盘模拟器模块
//
// 职责：按 KeyboardParams 重复发送一个热键组合（修饰键 + 主键）。
//
// 不做：
//   - 不做表单校验 —— 空热键（vk == 0 且 modifiers == 0）也照跑，
//     由 input 层记一条 "skipped, no events"；参数是否合法是面板的事
//   - 不认识 ThreadManager / 面板 / UI
//   - 循环体不打日志（热路径），只发事件；可观测性靠 input 层
//   - 不负责"发送一次"（那是面板的 SendOnce，UI 线程直调）
//
// 错误契约：input::SendInputError 往上抛，由 ThreadManager 的 launch 兜底
// ============================================================
namespace keyboard {

    // 返回实际执行轮数（= 实际按键次数）。
    // 被 stop 打断时已经抬起的键不会残留，返回也已经数过被打断的那一轮。
    [[nodiscard]] int run(const KeyboardParams &p, const ITimer &timer, const std::stop_token &st);

} // namespace keyboard
