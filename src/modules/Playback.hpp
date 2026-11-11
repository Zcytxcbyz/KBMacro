#pragma once

#include <stop_token>
#include <vector>

#include "common/Action.hpp"
#include "common/ITimer.hpp"
#include "common/Types.hpp"

// Playback: re-send a list of actions with their own time intervals.
//
// Contract:
//   - reads actions only, does not own or modify them
//   - does not know ThreadManager / panels
//   - the loop body does not log (the input layer already records each event)
//   - input::SendInputError is thrown outward, ThreadManager catches it as FinishReason::Failed
//   - returns the number of actions actually played (not rounds)
//
// ActionType notes:
//   - delayMs = "wait this long after the previous action fully finishes", so playback uses it as pure pre-wait
//   - MouseClick first moves to x/y and then clicks
//   - future relative coordinates will be a new ActionType value, this file treats unknown types as no-op
namespace playback {

[[nodiscard]] int
run(const std::vector<Action> &actions, const RunSpec &run, const ITimer &timer, const std::stop_token &st);

} // namespace playback
