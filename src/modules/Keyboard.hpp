#pragma once

#include <stop_token>

#include "common/ITimer.hpp"
#include "common/KeyboardParams.hpp"

// ============================================================
// Keyboard simulator module
//
// Responsibility: repeatedly send one hotkey combination (modifiers + main key) per KeyboardParams.
//
// Not doing:
//   - no form validation -- an empty hotkey (vk == 0 and modifiers == 0) still runs,
//     the input layer logs one "skipped, no events"; whether the params are valid is the panel's job
//   - does not know ThreadManager / panels / UI
//   - the loop body does not log (hot path), only sends events; observability comes from the input layer
//   - does not handle "send once" (that is the panel's SendOnce, called directly on the UI thread)
//
// Error contract: input::SendInputError is thrown upward, ThreadManager's launch catches it
// ============================================================
namespace keyboard {

    // Return the actual number of rounds executed (= actual key presses).
    // Keys already released before the stop interruption do not linger; the count includes the interrupted round.
    [[nodiscard]] int run(const KeyboardParams &p, const ITimer &timer, const std::stop_token &st);

} // namespace keyboard
