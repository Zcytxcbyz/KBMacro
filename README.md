# KBMacro

A portable Windows macro utility: a mouse auto-clicker, a keyboard simulator and
a recorder/player for input scripts. One executable, no installer — copy it to a
USB stick and run it.

GPL-3.0 · Windows 10/11 (x64)

## Features

**Mouse clicker** — click at a fixed position or wherever the cursor is, ten
click types (left/right/middle, double, four wheel directions, two side
buttons), wheel steps 1–10, configurable interval and hold time, and three run
modes (unlimited, timed, fixed count).

**Keyboard simulator** — send a key combination once or repeatedly, with
configurable interval and hold time.

**Recorder & playback** — capture mouse and keyboard input into an action list
that you can edit, reorder, export to JSON and play back. Supports drag
(`mouse down` → moves → `mouse up`), mouse moves, wheel, absolute and relative
coordinates, and explicit delay actions.

**Shared**

- Global hotkeys for every action, each one individually enabled
- Playback speed 0.25×–4× (scales delays and hold times, not the input
  actions themselves)
- Automatic coordinate adaptation when importing a script recorded on another
  monitor resolution
- Three timer modes: `Sleep` (low CPU), `Spin` (busy wait, highest precision)
  and `Hybrid` (sleep first, busy wait for the last few milliseconds)
- Chinese and English UI, switchable at runtime (requires restart)
- Portable mode: keep the configuration next to the executable
- Dry-run builds that log input actions instead of injecting them

## Requirements

- Windows 10 or 11, x64
- No runtime dependencies: wxWidgets, nlohmann-json, spdlog, WIL and
  moodycamel::ConcurrentQueue are all linked statically

## Using it

Start a task with its hotkey; the buttons in the window do the same thing.
Recording and playback are mutually exclusive: starting one while the other is
running is rejected rather than silently killing the first one.

### Default hotkeys

| Key | Action            | Key | Action             |
|-----|-------------------|-----|--------------------|
| F1  | Start mouse clicker | F7 | Stop recording    |
| F2  | Stop mouse clicker  | F8 | Start playback    |
| F3  | Start keyboard      | F9 | Stop playback     |
| F4  | Stop keyboard       | F10| Start all         |
| F5  | Send key once       | F11| Stop all          |
| F6  | Start recording     |    |                   |

Hotkeys are registered system-wide (`RegisterHotKey`), so they also work while
the window is minimised. Registration is exclusive: if another program already
owns the combination, KBMacro reports it in the status bar instead of silently
doing nothing.

### Data files

| What | Where |
|---|---|
| Configuration | `%APPDATA%\KBMacro\config.json` |
| Portable configuration | `KBMacro.exe.json` next to the executable (takes precedence) |
| Logs | `logs\KBMacro_<timestamp>.log` next to the executable |

Logs are written at `DEBUG` level by default in the `*-user` presets and the 30
most recent files are kept. Exported parameters and scripts use plain JSON:

- `*.clicker.json` — mouse clicker parameters
- `*.keyboard.json` — keyboard simulator parameters
- `*.macro.json` — action list plus the screen specification it was recorded on

## Build from source

Prerequisites: Visual Studio 2022 (or newer) with the C++ desktop workload,
CMake 3.25+, Conan 2, Ninja, and Python 3 (only for the helper scripts).

```powershell
# 1. dependencies (Debug and Release are separate folders)
conan install . --output-folder=build/debug   --build=missing -s build_type=Debug
conan install . --output-folder=build/release --build=missing -s build_type=Release

# 2. configure
cmake --preset x64-debug
cmake --preset x64-release

# 3. build
cmake --build build/debug
cmake --build build/release
```

The executable ends up in `build\<config>\bin\KBMacro.exe`.

### Presets

| Preset | Binary dir | Logging | Tests | Input |
|---|---|---|---|---|
| `x64-debug` | `build/debug` | off | off | real |
| `x64-release` | `build/release` | off | off | real |
| `x64-debug-user` | `build/debug-user` | on | on | dry run |
| `x64-release-user` | `build/release-user` | on | on | dry run |

The `*-user` presets come from `CMakeUserPresets.json`, which is not tracked by
git — copy and adjust them for your own workflow.

### CMake options

| Option | Default | Effect |
|---|---|---|
| `ENABLE_LOGGING` | `OFF` | Compile in `LOG_*` calls (no-op when off) |
| `ENABLE_TESTS` | `OFF` | Build the GoogleTest suite |
| `ENABLE_DRY_RUN` | `OFF` | Never call `SendInput`; log what would have been sent |

`ENABLE_DRY_RUN` is a compile-time switch on purpose: with it on, the injection
call is compiled out entirely, so a dry-run build cannot move your mouse no
matter what the code does. It is meant for testing the UI and the task/state
machinery without touching the desktop. Timings are not shortened — the delays
still happen, so the timing behaviour you see is the real one.

## Tests

```powershell
cmake --preset x64-debug-user
cmake --build build/debug-user
ctest --test-dir build/debug-user --output-on-failure
```

The suite covers the pure logic only — run modes, timing, action
serialisation, coordinate adaptation, hotkey text formatting, recording
normalisation and speed scaling. UI, hooks and `SendInput` are covered by the
manual smoke checklist instead.

## Tools

| Script | Purpose |
|---|---|
| `tools/build.py` | Sync the version into `CMakeLists.txt`/`resource.rc`/git tag, then configure and build |
| `tools/format_rc.py` | Align the strings in `res/langs/*.rc` |
| `tools/fetch_licenses.py` | Download the pinned third-party licenses into `res/licenses/` and regenerate `THIRD_PARTY_LICENSES.txt` |
| `tools/check_comments_only.py` | Verify a diff touches comments only |
| `tools/generate_keynames.py` | Regenerate `src/common/KeyNames.gen.hpp` (needs `pywin32`) |
| `tools/make_icon.py` | Regenerate `res/resources/KBMacro.ico` from a PNG (needs Pillow) |

## Known limitations

- Injected input is filtered, but input from other automation tools is only
  filtered when “ignore injected events” is enabled.
- IME-composed text (`VK_PACKET`) is never recorded — the recorder works at the
  key level, not the character level.
- Windows silently removes a low-level hook that is too slow; if that ever
  happens, recording keeps running but stops producing actions.
- Hotkeys are registered through `RegisterHotKey`, not a keyboard hook, so they
  cannot be captured while another process holds them.
- Playback speed scales delays and hold times, not the cost of the input
  actions themselves, so very short actions end up slower than the nominal
  multiplier.

## Third-party components

| Component | Version | License |
|---|---|---|
| wxWidgets | 3.2.11 | wxWindows Library Licence |
| nlohmann-json | 3.12.0 | MIT |
| spdlog | 1.17.0 | MIT |
| WIL | 1.0.260126.7 | MIT |
| moodycamel::ConcurrentQueue | 1.0.5 | BSD-2-Clause |
| GoogleTest | 1.18.0 | BSD-3-Clause |

Full texts are in `THIRD_PARTY_LICENSES.txt`, and are also embedded in the
executable (About → Components & Licenses).

## License

GPL-3.0. See `LICENSE.txt`.
