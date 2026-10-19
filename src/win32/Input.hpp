#pragma once

#include <stdexcept>
#include <stop_token>

#include "common/Action.hpp"   // MouseButton / WheelDirection
#include "common/ITimer.hpp"   // 复合层的按住等待
#include "common/Types.hpp"    // Hotkey

// ============================================================
// 模拟输入注入 —— Win32 SendInput 的唯一出口
//
// 职责：
//   - 把动作翻成等价的 INPUT，调 SendInput，检查返回值，记日志
//
// 不做：
//   - 不等待"动作间隔"（那是 RunController + ITimer 的事），只等"按住时长"
//   - 不做坐标适配（导入那一刻已完成），只把最终屏幕坐标归一化后交给 Win32
//   - 不认识 UI 词汇（ClickType 等），只说 Win32 词汇：
//     MouseButton / WheelDirection / Hotkey —— 正好是 Action 里那三个枚举
//   - 不产生停止请求、不碰线程；stop_token 由调用方给
//
// 错误契约：失败一律抛，接收方负责收
//   - 每次 SendInput 都检查返回值；小于预期 → 先补发抬起收拾现场 → 抛 SendInputError
//   - 线程任务由 ThreadManager 兜底（catch → FinishReason::Failed）
//   - UI 线程直调（键盘的 SendOnce）必须自己 try/catch，
//     否则异常穿透 wx 事件循环直接 terminate
//
// 日志契约：一次 SendInput 调用 = 一行 LOG_DEBUG（动作 + 关键参数 + sent/expected）
//
// 线程：纯 Win32 调用、无共享状态，任意线程可调；
//       但"同一时刻只有一个任务在跑"是 ThreadManager 的保证，不是这里的
// ============================================================

namespace input {

// SendInput 失败。what() 里已经带上 动作 / expected / sent / GetLastError() 文本，
// 直接丢进日志即可，不需要额外字段。
class SendInputError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// 启动时调一次：把虚拟屏幕 rect 与 DPI 记一条 LOG_DEBUG。
// 纯粹是日志（排查"坐标偏"时用来复算归一化），不建立任何状态。
void init();

// ---- 原子层：只发事件、立即返回（回放用它精确重放事件流）----
void moveTo(int x, int y);
void buttonDown(MouseButton button);
void buttonUp(MouseButton button);
void wheel(WheelDirection direction, int steps);   // steps 1–10，一条事件带 steps × WHEEL_DELTA
void keyDown(const Hotkey &hk);                    // 先修饰键、后主键
void keyUp(const Hotkey &hk);                      // 先主键、后修饰键

// ---- 复合层：语义完整，内部等 holdMs ----
// 返回 false = 按住期间被 stop 打断（键/鼠标键已经抬起，调用方 break 即可）
[[nodiscard]] bool click(MouseButton button, int holdMs, const ITimer &timer, const std::stop_token &st);
[[nodiscard]] bool pressKey(const Hotkey &hk, int holdMs, const ITimer &timer, const std::stop_token &st);

} // namespace input
