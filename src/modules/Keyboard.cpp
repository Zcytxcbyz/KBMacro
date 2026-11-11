#include "modules/Keyboard.hpp"

#include <algorithm> // std::max

#include "common/Log.hpp"
#include "common/RunController.hpp"

#include "win32/Input.hpp"

namespace keyboard {

int run(const KeyboardParams &p, const ITimer &timer, const std::stop_token &st) {
    // wait between rounds: intervalMs is the whole cycle, hold duration is taken out of it
    const int gapMs = std::max(0, p.intervalMs - p.holdMs);

    // start delay must come before RunController construction (its origin is the first round's action time)
    if (const int startDelayMs = std::max(0, p.run.startDelayMs); startDelayMs > 0) {
        if (!timer.wait(ITimer::Duration{startDelayMs}, st)) {
            LOG_DEBUG("keyboard: start delay interrupted, executed=0");
            return 0;
        }
    }

    RunController rc(p.run, ITimer::now());
    int executed = 0;

    while (rc.shouldRun(ITimer::now())) {
        if (st.stop_requested()) break; // stop requested: send no events at all in this round

        // pressKey returns false = interrupted by stop during hold (release already sent)
        const bool interrupted = !input::pressKey(p.hotkey, p.holdMs, timer, st);

        ++executed; // the interrupted round also counts: the key was sent and released
        rc.noteIteration();

        if (interrupted) break;
        if (!rc.shouldRun(ITimer::now())) break; // no idle wait for an interval after the last round

        if (!timer.wait(rc.clampWait(ITimer::Duration{gapMs}, ITimer::now()), st)) break;
    }

    LOG_DEBUG("keyboard: run finished, executed={}", executed);
    return executed;
}

} // namespace keyboard
