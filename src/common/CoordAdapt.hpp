#pragma once

#include <vector>

#include "common/Action.hpp"
#include "common/Types.hpp"

// coordinate adaptation: scale screen coordinates inside actions from one screen spec to another.
//
// only for importing others' *.macro.json — recording always uses absolute coordinates of this machine.
// pure data transformation: no threads, timers or Win32 input involved.
namespace coord {

// scale x and y of MouseMove / MouseClick proportionally:
//   newX = MulDiv(oldX, to.width,  from.width)
//   newY = MulDiv(oldY, to.height, from.height)
//
// MulDiv rounds to nearest (0.5 away from zero); integer division would bias scaled-down results low.
//
// when from width or height <= 0 nothing changes (an all-zero ScreenSpec means "not recorded").
// negative coordinates are scaled too, never clamped — a secondary screen left of the primary is negative.
// all other fields (wheelSteps / hotkey / keyEvent / holdMs / delayMs) are untouched.
void adaptActions(std::vector<Action> &actions, const ScreenSpec &from, const ScreenSpec &to);

} // namespace coord
