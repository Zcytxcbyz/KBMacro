#pragma once

#include <cstdint>
#include <vector>

#include "common/Action.hpp"

// Normalization of recorded events: raw events → Action list.
//
// deliberately does not depend on <windows.h>: normalization is a pure function, unit tests need no Win32
// environment, and it reads no config and logs nothing. how the hook side fills events is another module's job.

namespace recorder {

// Raw event types the hook callback hands to the normalizer
enum class RawKind {
    KeyDown,
    KeyUp,
    Move,
    ButtonDown,
    ButtonUp,
    Wheel,
};

struct RawEvent {
    RawKind kind = RawKind::Move;

    int x = 0; // position for mouse events (unused by keyboard events)
    int y = 0;
    MouseButton button = MouseButton::Left; // ButtonDown / ButtonUp

    unsigned vk = 0; // KeyDown / KeyUp
    int wheelDelta = 0; // Wheel: fraction of WHEEL_DELTA, positive = up/right
    std::uint32_t time = 0; // GetTickCount milliseconds, wraps (always diffed as unsigned)
    bool horizontal = false; // Wheel: horizontal (MOUSEEVENTF_HWHEEL)
};

// Policy bits used by normalization, one-to-one with Recorder's RecordOptions
struct NormalizeOptions {
    unsigned startKeyVk = 0; // start hotkey main key (0 = not configured)
    unsigned stopKeyVk = 0; // stop hotkey main key
    unsigned startMods = 0; // start hotkey modifier mask
    unsigned stopMods = 0; // stop hotkey modifier mask
    int maxActions = 100000; // 0 = unlimited
};

// head/tail stripping, main key filtering, auto-repeat merging, click/drag decision, wheel accumulation,
// delayMs computation, action limit -- all done here. an empty output means "nothing was recorded".
[[nodiscard]] std::vector<Action> normalize(const std::vector<RawEvent> &events, const NormalizeOptions &opts);

} // namespace recorder
