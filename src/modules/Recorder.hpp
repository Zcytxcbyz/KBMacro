#pragma once

#include <stop_token>
#include <vector>

#include "common/Action.hpp"
#include "common/ITimer.hpp"

// Keyboard/mouse recording -- low-level hook capture side.
//
// Thread contract:
//   run() must run on its own thread (ThreadManager's worker thread). Low-level hooks can only be
//   installed on a thread that pumps its own messages: on the UI thread, StopRecord's blocking join
//   leaves the hook callbacks undelivered, and after LowLevelHooksTimeout (300ms by default) the
//   system silently removes the hook, so nothing can be recorded afterwards.
//
// Input contract:
//   opts is read by the caller (UI thread) from ConfigManager + hotkey config and passed in; this module
//   reads no singletons and no global state, the thread only knows this one parameter struct.
//
// Output contract:
//   - being stopped by stop, hitting the action limit, and recording no event at all are normal returns (no throw)
//   - hook installation failure throws (wil::ResultException), ThreadManager records Failed
//   - an empty list means nothing was recorded, the panel decides what to show
//
// Convention: no ThreadManager, no UI here; logging is only first/last lines + one progress line per second.
namespace recorder {

    // Recording options. Filled in entirely by the UI thread, read-only on the recording thread.
    struct RecordOptions {
        int startDelayMs = 0;      // wait this long before installing hooks, to skip the start hotkey's own press
        int moveIntervalMs = 16;     // mouse move sampling interval; 0 = no sampling (record all)
        bool ignoreInjected = false; // also skip events injected by other programs (own events never recorded)
        int maxActions = 100000;     // action limit; 0 = unlimited

        // Main keys to drop while recording: every enabled global hotkey.
        // these keys are already consumed by the hotkey system during recording and never reach the foreground
        // app; recording them would only trigger ourselves during playback (an F1 in the output starts the clicker).
        std::vector<unsigned> ignoredKeys;

        // start/stop hotkey main key and modifiers: used only by normalization to strip the modifier runs
        // at the head and tail of the output (0 = not configured). main keys are filtered via ignoredKeys above.
        unsigned startKeyVk = 0;
        unsigned startMods = 0;
        unsigned stopKeyVk = 0;
        unsigned stopMods = 0;
    };

    // Record until stopped or the limit is reached, return the normalized action list (may be empty).
    [[nodiscard]] std::vector<Action> run(const RecordOptions &opts, const ITimer &timer,
                                          const std::stop_token &st);

} // namespace recorder
