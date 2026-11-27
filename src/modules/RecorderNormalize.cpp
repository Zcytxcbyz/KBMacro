#include "modules/RecorderNormalize.hpp"

#include <cstdlib>
#include <optional>
#include <unordered_set>
#include <utility>

#include "common/Hotkey.hpp"

namespace {

// movement beyond this many pixels counts as a drag, otherwise it collapses into one click
constexpr int kDragThreshold = 2;

// granularity of one wheel notch
constexpr int kWheelDelta = 120;

// a press longer than this is not folded: an hour is far beyond "typing", and folding it would hide a
// genuinely long hold behind one row (keep the raw Down/Up pair instead of shortening the data)
constexpr int kMaxFoldHoldMs = 3600000;

// whether this event is "a modifier key of some hotkey"
[[nodiscard]] bool isModifierEventOf(const recorder::RawEvent &e, const unsigned mask) noexcept {
    if (mask == 0) return false;
    if (e.kind != recorder::RawKind::KeyDown && e.kind != recorder::RawKind::KeyUp) return false;

    const unsigned bit = utils::modifierBitForVk(e.vk);
    return bit != 0 && (bit & mask) != 0;
}

} // namespace

namespace recorder {

std::vector<Action> normalize(const std::vector<RawEvent> &events, const NormalizeOptions &opts) {
    std::vector<Action> out;
    if (events.empty()) return out;

    // ---- 1. head/tail stripping ----
    // head is "modifier keys of the start hotkey" (the user pressed Ctrl etc. before pressing the hotkey),
    // tail is "modifier keys of the stop hotkey" (the hotkey already stopped the thread, releases are its echo).
    std::size_t begin = 0;
    std::size_t end = events.size();

    while (begin < end && isModifierEventOf(events[begin], opts.startMods))
        ++begin;
    while (end > begin && isModifierEventOf(events[end - 1], opts.stopMods))
        --end;

    // ---- 2. per-event normalization ----
    bool haveLast = false;
    std::uint32_t lastTime = 0;

    const auto emit = [&](Action action, const std::uint32_t time) {
        action.delayMs = haveLast ? static_cast<int>(time - lastTime) : 0;
        haveLast = true;
        lastTime = time;
        out.push_back(std::move(action)); // NOLINT(*-move-const-arg)
    };

    // a keyboard Down waiting for its Up: not emitted yet, so the pair can still fold into one press
    std::optional<Action> pendingDown;
    std::uint32_t pendingDownTime = 0;

    const auto flushPendingDown = [&] {
        if (!pendingDown) return;
        emit(*pendingDown, pendingDownTime);
        pendingDown.reset();
    };

    // anything else that reaches the output closes the fold window first, so the order stays chronological
    const auto push = [&](Action action, const std::uint32_t time) {
        flushPendingDown();
        emit(std::move(action), time);
    };

    const auto limitReached = [&] { return opts.maxActions > 0 && static_cast<int>(out.size()) >= opts.maxActions; };

    // keyboard: keys still held down (auto-repeat merge + drop Up without matching Down)
    std::unordered_set<unsigned> keysDown;

    // mouse: pending press (may collapse into a click, or escalate into a drag)
    bool havePendingDown = false;
    MouseButton pendingButton = MouseButton::Left;
    int pendingX = 0;
    int pendingY = 0;
    std::uint32_t pendingTime = 0;
    bool dragging = false;

    // mouse: buttons already emitted as MouseDown (for multi-button press), so Up can be paired
    std::unordered_set<MouseButton> emittedButtons;

    // wheel: leftover below one notch
    int wheelPending = 0;
    bool wheelHorizontal = false;

    for (std::size_t i = begin; i < end && !limitReached(); ++i) {
        const RawEvent &e = events[i];

        // ---- keyboard ----
        if (e.kind == RawKind::KeyDown || e.kind == RawKind::KeyUp) {
            // start/stop hotkey main keys never enter the output (the hook already filters them, this is the
            // second pass, and it also makes this function self-contained and unit-testable)
            if ((opts.startKeyVk != 0 && e.vk == opts.startKeyVk) || (opts.stopKeyVk != 0 && e.vk == opts.stopKeyVk)) {
                continue;
            }

            if (e.kind == RawKind::KeyDown) {
                // auto-repeat: holding the key sends repeated Down, keep only the first
                if (!keysDown.insert(e.vk).second) continue;

                // a second Down (chord, or the same key again) ends the window: emit the old one unchanged
                flushPendingDown();
                pendingDown = Action{.type = ActionType::Keyboard,
                                     .hotkey = Hotkey{.modifiers = 0, .vk = e.vk},
                                     .keyEvent = KeyEventType::Down};
                pendingDownTime = e.time;
                continue;
            }

            // an Up without a matching Down (e.g. held before recording started) is dropped outright,
            // otherwise playback would release a key that was never pressed
            if (keysDown.erase(e.vk) == 0) continue;

            // nothing was emitted in between and the span is sane: one press instead of Down + Up
            if (pendingDown && pendingDown->hotkey.vk == e.vk) {
                const auto hold = static_cast<int>(e.time - pendingDownTime);
                if (hold <= kMaxFoldHoldMs) {
                    Action press = *pendingDown;
                    press.type = ActionType::KeyboardPress;
                    press.holdMs = hold;
                    press.delayMs = haveLast ? static_cast<int>(pendingDownTime - lastTime) : 0;
                    haveLast = true;
                    lastTime = e.time; // the press covers the whole span
                    pendingDown.reset();
                    out.push_back(std::move(press));
                    continue;
                }
            }

            flushPendingDown();
            push(Action{.type = ActionType::Keyboard,
                        .hotkey = Hotkey{.modifiers = 0, .vk = e.vk},
                        .keyEvent = KeyEventType::Up},
                 e.time);
            continue;
        }

        // ---- mouse move ----
        if (e.kind == RawKind::Move) {
            if (havePendingDown && !dragging) {
                if (std::abs(e.x - pendingX) <= kDragThreshold && std::abs(e.y - pendingY) <= kDragThreshold) {
                    continue; // jitter, not a drag
                }

                // threshold crossed: emit the press (start point) first, then this move
                push(Action{.type = ActionType::MouseDown, .x = pendingX, .y = pendingY, .button = pendingButton},
                     pendingTime);
                emittedButtons.insert(pendingButton);
                dragging = true;
            }

            push(Action{.type = ActionType::MouseMove, .x = e.x, .y = e.y}, e.time);
            continue;
        }

        // ---- mouse down ----
        if (e.kind == RawKind::ButtonDown) {
            if (havePendingDown) {
                // a button is already down: emit this one directly, it does not join click merging (multi-press)
                push(Action{.type = ActionType::MouseDown, .x = e.x, .y = e.y, .button = e.button}, e.time);
                emittedButtons.insert(e.button);
                continue;
            }

            havePendingDown = true;
            dragging = false;
            pendingButton = e.button;
            pendingX = e.x;
            pendingY = e.y;
            pendingTime = e.time;
            continue;
        }

        // ---- mouse up ----
        if (e.kind == RawKind::ButtonUp) {
            if (havePendingDown && e.button == pendingButton) {
                const bool moved = dragging || std::abs(e.x - pendingX) > kDragThreshold ||
                                   std::abs(e.y - pendingY) > kDragThreshold;

                if (moved) {
                    if (!dragging) {
                        // dragged in one step with no Move event in between: emit the start point
                        push(Action{.type = ActionType::MouseDown,
                                    .x = pendingX,
                                    .y = pendingY,
                                    .button = pendingButton},
                             pendingTime);
                    }
                    push(Action{.type = ActionType::MouseUp, .x = e.x, .y = e.y, .button = e.button}, e.time);
                } else {
                    // pressed and released in place: collapse into one click, hold duration is the real span
                    auto click = Action{
                            .type = ActionType::MouseClick, .x = pendingX, .y = pendingY, .button = pendingButton};
                    click.holdMs = static_cast<int>(e.time - pendingTime);
                    push(click, e.time);
                }

                havePendingDown = false;
                dragging = false;
                continue;
            }

            // not the pending button: emit Up only if this button really produced a MouseDown
            if (emittedButtons.erase(e.button) > 0) {
                push(Action{.type = ActionType::MouseUp, .x = e.x, .y = e.y, .button = e.button}, e.time);
            }
            continue;
        }

        // ---- wheel ----
        if (e.kind == RawKind::Wheel) {
            if (wheelHorizontal != e.horizontal && wheelPending != 0) {
                // on a horizontal/vertical switch the sub-notch leftover is dropped (rare, no rounding for the user)
                wheelPending = 0;
            }
            wheelHorizontal = e.horizontal;
            wheelPending += e.wheelDelta;

            while (std::abs(wheelPending) >= kWheelDelta) {
                const int notches = wheelPending / kWheelDelta;
                wheelPending -= notches * kWheelDelta;

                const bool positive = notches > 0;
                const WheelDirection dir = wheelHorizontal ? (positive ? WheelDirection::Right : WheelDirection::Left)
                                                           : (positive ? WheelDirection::Up : WheelDirection::Down);

                push(Action{.type = ActionType::MouseWheel, .wheelDirection = dir, .wheelSteps = std::abs(notches)},
                     e.time);

                if (limitReached()) return out;
            }
        }
    }

    // a waiting Down still goes out unchanged (it really happened); the missing Up is left to playback's
    // release stuck keys. Sub-notch wheel leftovers and a still-held button invent nothing.
    if (opts.maxActions <= 0 || static_cast<int>(out.size()) < opts.maxActions) flushPendingDown();
    return out;
}

} // namespace recorder
