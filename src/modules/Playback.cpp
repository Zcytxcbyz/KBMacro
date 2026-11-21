#include "modules/Playback.hpp"

#include <algorithm>
#include <unordered_map>

#include "common/Log.hpp"
#include "common/RunController.hpp"
#include "win32/Input.hpp"

namespace {

// One entry of the key matching table: the full Hotkey (sent back as-is to release it) + press count.
struct HeldKey {
    Hotkey hotkey;
    int count = 0;
};

// Key of the matching table: vk in the low 16 bits, modifiers in the high bits.
// with vk == 0 (modifier-only press) it also never collides with another combination.
[[nodiscard]] unsigned heldKeyOf(const Hotkey &hk) noexcept {
    return (hk.vk & 0xFFFFu) | (hk.modifiers << 16);
}

// Release every key still held down.
// Normally the table is empty; having work to do means the script itself is unbalanced, or a stop/timed
// cutoff landed exactly between down and up -- without this fix the key would stay stuck.
//   loud: true only on thread exit. A release at round end is a routine safety net (an unbalanced
//         script repeats it every round), so it must not flood the log at warning level.
void releaseHeldKeys(std::unordered_map<unsigned, HeldKey> &held, const bool loud) {
    int released = 0;
    for (auto &val : held | std::views::values) {
        auto &[hotkey, count] = val;
        if (count <= 0) continue;

        input::keyUp(hotkey);
        count = 0;
        ++released;
    }

    if (released == 0) {
        return;
    }
    if (loud) {
        LOG_WARN("playback: released {} stuck key(s)", released);
    } else {
        LOG_DEBUG("playback: released {} stuck key(s) at round end", released);
    }
}

// Mouse button matching table: MouseDown adds a record, MouseUp clears it.
// key is the button itself, value is the press count (repeated down on one button does not release early).
void pressHeldButton(std::unordered_map<MouseButton, int> &held, const MouseButton button) {
    ++held[button];
}

// Release every mouse button still held down (once per round end and once on thread exit).
// loud has the same meaning as in releaseHeldKeys().
void releaseHeldButtons(std::unordered_map<MouseButton, int> &held, const bool loud) {
    int released = 0;
    for (auto &[button, count] : held) {
        if (count <= 0) continue;

        input::buttonUp(button);
        count = 0;
        ++released;
    }

    if (released == 0) {
        return;
    }
    if (loud) {
        LOG_WARN("playback: released {} stuck mouse button(s)", released);
    } else {
        LOG_DEBUG("playback: released {} stuck mouse button(s) at round end", released);
    }
}

// Move the cursor to the position given by the action:
//   relative == true  -> offset from the current position (moveBy, no coordinate adaptation)
//   relative == false -> absolute virtual-desktop coordinates (moveTo)
void positionCursor(const Action &action) {
    if (action.relative) {
        input::moveBy(action.x, action.y);
    } else {
        input::moveTo(action.x, action.y);
    }
}

// Apply one action. Returns false = interrupted by stop (only click's hold wait can do that).
bool applyAction(const Action &action,
                 const ITimer &timer,
                 const std::stop_token &st,
                 std::unordered_map<unsigned, HeldKey> &held,
                 std::unordered_map<MouseButton, int> &heldButtons) {
    switch (action.type) {
        case ActionType::MouseMove:
            positionCursor(action);
            return true;

        case ActionType::MouseClick:
            // recorded output always carries a preceding MouseMove, but a hand-written or edited script
            // may hold only one click, so position here too (the coordinates live in the Action anyway).
            positionCursor(action);
            return input::click(action.button, action.holdMs, timer, st);

        case ActionType::MouseWheel:
            input::wheel(action.wheelDirection, action.wheelSteps);
            return true;

        case ActionType::MouseDown:
            // the drag start must be positioned too (same reason as MouseClick)
            positionCursor(action);
            input::buttonDown(action.button);
            pressHeldButton(heldButtons, action.button);
            return true;

        case ActionType::MouseUp:
            // move to the release point first, otherwise the end point drifts back to the press position
            positionCursor(action);
            input::buttonUp(action.button);
            if (const auto it = heldButtons.find(action.button); it != heldButtons.end() && it->second > 0) {
                --it->second;
            }
            return true;

        case ActionType::Keyboard: {
            if (action.keyEvent == KeyEventType::Down) {
                input::keyDown(action.hotkey);
                auto &item = held[heldKeyOf(action.hotkey)];
                item.hotkey = action.hotkey;
                ++item.count;
            } else {
                input::keyUp(action.hotkey);
                if (const auto it = held.find(heldKeyOf(action.hotkey)); it != held.end() && it->second.count > 0) {
                    --it->second.count;
                }
            }
            return true;
        }

        case ActionType::Delay:
            // pure wait: the duration is delayMs, and delayMs was already performed as the
            // "pre-action wait" in the loop, so there is nothing to do here.
            // it exists as its own type because it is a visible row in the action list --
            // inserting a pause does not require editing the delay values of neighbouring actions.
            return true;
    }

    // all types are covered, this is only a fallback
    return true;
}

} // namespace

namespace playback {

int run(const std::vector<Action> &actions, const RunSpec &run, const ITimer &timer, const std::stop_token &st) {
    if (actions.empty()) {
        LOG_WARN("playback: empty action list, nothing to play");
        return 0;
    }

    // start delay: once per whole task, and it must come before RunController construction --
    // otherwise the timed mode duration would count this delay in.
    if (const int startDelayMs = std::max(0, run.startDelayMs); startDelayMs > 0) {
        if (!timer.wait(ITimer::Duration{startDelayMs}, st)) {
            LOG_DEBUG("playback: start delay interrupted, executed=0");
            return 0;
        }
    }

    RunController rc(run, ITimer::now());

    std::unordered_map<unsigned, HeldKey> held;
    std::unordered_map<MouseButton, int> heldButtons;
    int executed = 0;
    bool interrupted = false;

    while (!interrupted && rc.shouldRun(ITimer::now())) {
        for (const Action &action : actions) {
            if (st.stop_requested()) {
                interrupted = true;
                break;
            }

            // delayMs = wait this long after the previous action finishes, so this is pure pre-wait.
            if (action.delayMs > 0 && !timer.wait(rc.clampWait(ITimer::Duration{action.delayMs}, ITimer::now()), st)) {
                interrupted = true;
                break;
            }

            if (!applyAction(action, timer, st, held, heldButtons)) {
                interrupted = true;
                break;
            }

            ++executed;
        }

        if (interrupted) break;

        // release stuck keys at each round end so the next round starts with every key up.
        releaseHeldKeys(held, /*loud=*/false);
        releaseHeldButtons(heldButtons, /*loud=*/false);
        rc.noteIteration();

        if (st.stop_requested()) break;
    }

    // exit fallback: a stop interruption may leave keys/mouse buttons held down
    releaseHeldKeys(held, /*loud=*/true);
    releaseHeldButtons(heldButtons, /*loud=*/true);

    LOG_DEBUG("playback: run finished, executed={}", executed);
    return executed;
}

} // namespace playback
