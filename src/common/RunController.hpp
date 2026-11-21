#pragma once

#include "common/ITimer.hpp"
#include "common/Types.hpp"

// ============================================================
// run mode decider
//
// pure logic: no time source, no sleep, no stop_token, no UI/threads.
//   - "stop" comes from ITimer::wait's return value and is orthogonal to this class
//   - "now" is passed in by the caller (ITimer::now()), so tests can inject time points
//
// one per worker thread, constructed inside the call (not passed as a thread function parameter).
//
// usage (the loop skeleton shared by the three modules):
//   RunController rc(p.run, ITimer::now());
//   while (rc.shouldRun(ITimer::now())) {
//       doOneAction();
//       rc.noteIteration();
//       if (!timer->wait(rc.clampWait(Duration(p.intervalMs), ITimer::now()), st))
//           break;                    // stopped: leave the loop
//   }
// ============================================================

class RunController {
public:
    using TimePoint = ITimer::TimePoint;
    using Duration = ITimer::Duration;

    // start comes from the caller (usually ITimer::now() at thread start).
    // the Timed deadline is fixed at start + durationSec, no accumulated timing.
    RunController(const RunSpec &spec, TimePoint start);

    // may another iteration run?
    //   Infinite: always true
    //   Timed   : now < start + durationSec (strict less-than)
    //   Counted : executed iterations < count
    [[nodiscard]] bool shouldRun(TimePoint now) const;

    // how long should this iteration actually wait?
    //   Infinite / Counted: return interval unchanged
    //   Timed             : min(interval, time left to the deadline); 0 once past it
    //
    // why clamp: with interval 60s + timed 10s, waiting the full 60s would make
    // the task run 60s before it ends - "timed 10 sec" would be a lie.
    [[nodiscard]] Duration clampWait(Duration interval, TimePoint now) const;

    // one iteration done, keep count (Counted increments it here)
    void noteIteration();

private:
    RunMode m_mode = RunMode::Infinite;
    TimePoint m_start{};
    Duration m_duration{}; // Timed: total duration
    int m_totalCount = 0; // Counted: target iteration count
    int m_executedCount = 0; // Counted: completed iterations
};
