#pragma once

#include <stop_token>

#include "common/Action.hpp" // MouseButton / WheelDirection
#include "common/ClickerParams.hpp" // ClickType / ClickerParams
#include "common/ITimer.hpp"

// ============================================================
// Mouse auto-clicker -- runs a ClickerParams into real input events
//
// Responsibility:
//   - loops at RunController's pace, sending one input per round by ClickType (two for double-click)
//   - returns how many rounds actually ran, the panel uses it to show "completed N times"
//
// Not doing:
//   - does not manage threads or state, does not produce stop requests (stop_token comes from the caller)
//   - does not touch UI, does not know ThreadManager, does not know panels
//   - the loop body does not log (hot path); each SendInput is logged by win32/Input itself
//
// Error contract: input::SendInputError is thrown straight out, ThreadManager records Failed as a fallback
// ============================================================

namespace clicker {

// Expand ClickType into "what to send, how many times". Pure function, testable on its own.
//   - double-click = two full clicks (each waits holdMs), times = 2
//   - wheel types always use times = 1 (one event carries steps x WHEEL_DELTA)
struct ClickStep {
    bool wheel = false; // true: use direction + wheelSteps
    int times = 1; // how many times to send
    MouseButton button = MouseButton::Left; // valid when wheel == false
    WheelDirection direction = WheelDirection::Up; // valid when wheel == true
};

[[nodiscard]] ClickStep toClickStep(ClickType type) noexcept;

// Clicker main loop, returns the actual number of rounds executed.
//   - returns immediately when interrupted by stop, the value is the completed rounds
//   - per round: first moveTo (unless ignorePosition) -> then send input by ClickType
//   - wait between rounds = max(0, intervalMs - holdMs) (interval is the cycle, hold time deducted);
//     wheel types have no hold, wait = intervalMs
[[nodiscard]] int run(const ClickerParams &p, const ITimer &timer, const std::stop_token &st);

} // namespace clicker
