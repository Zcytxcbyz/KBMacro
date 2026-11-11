#include "modules/Clicker.hpp"

#include <algorithm>   // std::max

#include "common/Log.hpp"
#include "common/RunController.hpp"

#include "win32/Input.hpp"

namespace clicker {

ClickStep toClickStep(const ClickType type) noexcept {
    switch (type) {
        case ClickType::Left:
            return {.button = MouseButton::Left};
        case ClickType::Right:
            return {.button = MouseButton::Right};
        case ClickType::Middle:
            return {.button = MouseButton::Middle};

        // double-click = two full clicks (each waits holdMs), not the system double-click time limit
        case ClickType::Double:
            return {.times = 2, .button = MouseButton::Left};

        case ClickType::Side1:
            return {.button = MouseButton::Side1};
        case ClickType::Side2:
            return {.button = MouseButton::Side2};

        case ClickType::WheelUp:
            return {.wheel = true, .direction = WheelDirection::Up};
        case ClickType::WheelDown:
            return {.wheel = true, .direction = WheelDirection::Down};
        case ClickType::WheelLeft:
            return {.wheel = true, .direction = WheelDirection::Left};
        case ClickType::WheelRight:
            return {.wheel = true, .direction = WheelDirection::Right};
    }

    return {};   // unreachable: all enum branches covered, kept to keep the compiler quiet
}

int run(const ClickerParams &p, const ITimer &timer, const std::stop_token &st) {
    const auto [wheel, times, button, direction] =
        toClickStep(p.clickType);

    // wait between rounds: intervalMs is the whole cycle, hold duration is taken out of it;
    // wheel has no "hold", so the wait is the full interval.
    const int gapMs = wheel ? p.intervalMs : std::max(0, p.intervalMs - p.holdMs);

    // start delay: after the hotkey press the hand must release it and switch windows, so wait a bit first.
    // must come before RunController construction -- its origin is "when the first round's action starts",
    // counting the delay into the origin would eat the timed mode for nothing (5s delay + 5s timer = ends at once).
    if (const int startDelayMs = std::max(0, p.run.startDelayMs); startDelayMs > 0) {
        if (!timer.wait(ITimer::Duration{startDelayMs}, st)) {
            LOG_DEBUG("clicker: start delay interrupted, executed=0");
            return 0;
        }
    }

    RunController rc(p.run, ITimer::now());
    int executed = 0;

    while (rc.shouldRun(ITimer::now())) {
        // stop requested: send no events at all in this round.
        // (input::click checks too, but blocking here first is what makes the round count accurate)
        if (st.stop_requested()) break;

        // position first each round; with "ignore coordinates" checked the cursor is left alone
        if (!p.ignorePosition) input::moveTo(p.x, p.y);

        // times is only 1 (single-click types) or 2 (double-click): double-click = two full clicks, each holds holdMs.
        // the count comes from toClickStep, ClickType never appears in the loop body.
        bool interrupted = false;
        for (int i = 0; i < times; ++i) {
            if (wheel) {
                input::wheel(direction, p.wheelSteps);
            } else if (!input::click(button, p.holdMs, timer, st)) {
                interrupted = true;   // stopped during hold: release already sent
                break;
            }
        }

        // the interrupted round also counts: the press (or wheel) was already sent
        ++executed;
        rc.noteIteration();

        if (interrupted) break;

        // no need to wait another interval after the last round: Counted is full, Timed has no time left,
        // one more wait only delays the "completed" notice for nothing (60s interval means 60 seconds).
        if (!rc.shouldRun(ITimer::now())) break;

        if (!timer.wait(rc.clampWait(ITimer::Duration{gapMs}, ITimer::now()), st)) break;
    }

    LOG_DEBUG("clicker: run finished, executed={}", executed);
    return executed;
}

} // namespace clicker
