#pragma once

#include <cstdint>
#include <stdexcept>
#include <stop_token>

#include "common/Action.hpp" // MouseButton / WheelDirection
#include "common/ITimer.hpp" // hold wait for the composite layer
#include "common/Types.hpp" // Hotkey

// ============================================================
// Simulated input injection -- the only exit for Win32 SendInput
//
// Responsibilities:
//   - turn an action into an equivalent INPUT, call SendInput, check the return value, log it
//
// Not responsible for:
//   - not waiting for the "action interval" (that is RunController + ITimer's job), only the "hold duration"
//   - no coordinate adaptation (done at import time), only normalize the final screen coordinate for Win32
//   - no UI vocabulary (ClickType etc.), only Win32 vocabulary:
//     MouseButton / WheelDirection / Hotkey -- exactly the three enums in Action
//   - no stop request, no thread handling; stop_token is provided by the caller
//
// Error contract: failures always throw, the receiver handles them
//   - every SendInput checks its return value; fewer than expected -> re-send the up to clean up -> throw SendInputError
//   - thread tasks are covered by ThreadManager (catch -> FinishReason::Failed)
//   - direct calls on the UI thread (keyboard's SendOnce) must try/catch themselves,
//     otherwise the exception crosses the wx event loop and terminates
//
// Log contract: one SendInput call = one LOG_DEBUG line (action + key params + sent/expected)
//
// Threading: pure Win32 calls, no shared state, callable from any thread;
//            but "only one task runs at a time" is ThreadManager's guarantee, not this file's
// ============================================================

namespace input {

// SendInput failure. what() already carries the action / expected / sent / GetLastError() text,
// just drop it into the log, no extra fields needed.
class SendInputError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Identity marker for injected events ('KBMR'). Written into INPUT::dwExtraInfo,
// the recording side's low-level hook uses it to recognize "we sent this", so it never records itself.
inline constexpr std::uintptr_t kInjectedSignature = 0x4B424D52u;

// Called once at startup: log the virtual screen rect and DPI in one LOG_DEBUG line.
// purely for the log (used to recompute normalization when debugging "coordinates are off"), builds no state.
void init();

// ---- atomic layer: send events only, return at once (playback uses it to replay the event stream) ----
void moveTo(int x, int y);
// Move dx/dy physical pixels relative to the current position (may be negative). No virtual-desktop
// normalization, and no screen size from init() is needed.
void moveBy(int dx, int dy);
void buttonDown(MouseButton button);
void buttonUp(MouseButton button);
void wheel(WheelDirection direction, int steps); // steps 1-10, one event carries steps x WHEEL_DELTA
void keyDown(const Hotkey &hk); // modifiers first, main key after
void keyUp(const Hotkey &hk); // main key first, modifiers after

// ---- composite layer: full semantics, waits holdMs inside ----
// returns false = interrupted by stop during the hold (keys/buttons are already up, the caller can just break)
[[nodiscard]] bool click(MouseButton button, int holdMs, const ITimer &timer, const std::stop_token &st);
[[nodiscard]] bool pressKey(const Hotkey &hk, int holdMs, const ITimer &timer, const std::stop_token &st);

} // namespace input
