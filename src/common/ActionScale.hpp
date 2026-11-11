#pragma once

#include <vector>

#include "common/Action.hpp"

// scale the time fields of an action sequence by a factor (used for the playback factor).
//
//   factor > 1  playback speeds up, times get shorter
//   factor < 1  playback slows down, times get longer
//   factor == 1 unchanged (no numeric change at all)
//
// only delayMs and holdMs change; coordinates, buttons and wheel steps stay as is.
// after scaling holdMs is clamped to >= kMinHoldMs: when too fast the system merges down/up into one click.
// after scaling delayMs is clamped to >= 0.
namespace action {

// lower bound of the hold duration (ms). Below it down and up may show no observable gap.
inline constexpr int kMinHoldMs = 5;

// scale in place. does nothing when factor <= 0 or == 1.
void scaleActions(std::vector<Action> &actions, double factor);

} // namespace action
