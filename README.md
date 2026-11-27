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
coordinates, explicit delay actions, and single key presses. A key down/up pair
with nothing in between is stored as one `key press` action carrying its own
hold time; chords such as Ctrl+C stay as separate down/up actions, so a key can
be held across other events.

**Shared**

- Global hotkeys for every action, each one individually enabled
- Playback speed 0.25×–4× (scales delays and hold times, not the input
  actions themselves)
- Automatic coordinate adaptation when importing a script recorded on another
  monitor resolution
- Three timer modes: `Sleep` (low CPU), `Spin` (busy wait, highest precision)
  and `Hybrid` (sleep first, busy wait for the last few milliseconds)
- Nine UI languages (English, 简体中文, 繁體中文, Deutsch, Français, 日本語,
  Italiano, Español, Русский), switchable at runtime (requires restart)
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

Logs are written only when the build enables `ENABLE_LOGGING` — the presets in
this repository leave it off. The 30 most recent files are kept. Exported
parameters and scripts use plain JSON:

- `*.clicker.json` — mouse clicker parameters
- `*.keyboard.json` — keyboard simulator parameters
- `*.macro.json` — action list plus the screen specification it was recorded on

### UI languages

The UI ships in nine languages: English, 简体中文, 繁體中文, Deutsch, Français,
日本語, Italiano, Español and Русский. Pick one in Settings; it takes effect
after a restart.

Each language is a JSON table in `res/langs/<code>.json`, embedded in the
executable as `RCDATA`. English and Simplified Chinese also keep a Win32 string
table (`res/langs/*.rc`) as a fallback.

Lookup order at runtime:

1. the JSON table of the current language
2. its string table (only en-US and zh-CN have one)
3. the English string table
4. an empty string, with one warning in the log

A partially translated file therefore still works, but a complete one is what
you want: `i18n: loaded N entr(ies) from JSON resource 21x` should report the
same N as `en-US.json` (204 at the time of writing) and no `WARN` should follow
in the same run.

To add a language:

1. copy `res/langs/en-US.json` and translate the values — the keys are string
   ids, do not rename them
2. add an `IDR_LANG_*` id in `res/resource.h`
3. register the file as `RCDATA` in `res/resource.rc`
4. add one entry to `kLangs[]` in `res/lang.h`

`tools/lang_json.py` keeps the JSON tables and the fallback `.rc` tables in
sync:

```powershell
python tools/lang_json.py export   # .rc  -> .json
python tools/lang_json.py import   # .json -> .rc (only the quoted text changes)
python tools/lang_json.py check    # report drift, write nothing
```

CMake does not track the files referenced by `RCDATA`, so after editing a JSON
table refresh the resource script before building:

```powershell
(Get-Item 'res\resource.rc').LastWriteTime = Get-Date
```

## Build from source

Prerequisites: Visual Studio 2022 (or newer) with the C++ desktop workload,
CMake 4.0+, Conan 2, Ninja, and Python 3 (for the helper scripts). Run every
command below from a **Developer PowerShell** for Visual Studio — `cl.exe` and
`ninja` only exist inside that environment.

### One command

```powershell
python tools\build.py
```

It syncs the version into `CMakeLists.txt` and `res/resource.rc`, runs
`conan install`, configures and builds the `x64-release` preset, then prints the
executable's path, its size and the file version read back from the binary —
which also proves both version numbers made it into the resources.

| Flag | Effect |
|---|---|
| `--preset x64-debug` | build another preset (default: `x64-release`) |
| `--version 1.2.3` | write that version into both files, then build |
| `--check` | only compare tag / `CMakeLists.txt` / `resource.rc`, exit 1 on mismatch |
| `--tag` | after a successful build, create the annotated tag `v<version>` |
| `--no-build` | sync the version files and stop |
| `--dry-run` | print what would change, write nothing |

The script does not run the tests, so do that separately. The presets here leave
`ENABLE_TESTS` off, so turn it on for one configure pass:

```powershell
cmake --preset x64-debug -DENABLE_TESTS=ON
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

### By hand

The same three steps, spelled out. Debug and Release use separate build trees
and separate Conan output folders:

```powershell
# 1. dependencies (Debug and Release are separate folders)
conan install . --output-folder=build/debug --build=missing --profile=profiles/debug
conan install . --output-folder=build/release --build=missing --profile=profiles/release

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

Both keep logging and tests off, so a plain `cmake --preset` behaves like a
release build. For development you want them on — either override once:

```powershell
cmake --preset x64-debug -DENABLE_LOGGING=ON -DENABLE_TESTS=ON
```

or create your own preset in `CMakeUserPresets.json` (not tracked by git, so it
stays local):

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "dev-debug",
      "inherits": "x64-debug",
      "binaryDir": "${sourceDir}/build/dev-debug",
      "cacheVariables": {
        "ENABLE_LOGGING": "ON",
        "ENABLE_TESTS": "ON"
      }
    }
  ]
}
```

`inherits` carries the Conan toolchain path over, so run the `conan install` for
`build/debug` first. Add `"ENABLE_DRY_RUN": "ON"` to the same block for a build
that logs what it would send but never calls `SendInput`.

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
cmake --preset x64-debug -DENABLE_TESTS=ON
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

185 cases, a few seconds on a modern machine. The suite covers the pure logic
only — run modes, timing, action serialisation, coordinate adaptation, hotkey
text formatting, recording normalisation and speed scaling. UI, hooks and
`SendInput` are covered by the manual smoke checklist instead.

## Tools

| Script | Purpose |
|---|---|
| `tools/build.py` | Sync the version into `CMakeLists.txt`/`resource.rc`/git tag, then configure and build |
| `tools/fetch_licenses.py` | Download the pinned third-party licenses into `res/licenses/` and regenerate `THIRD_PARTY_LICENSES.txt` |
| `tools/generate_keynames.py` | Regenerate `src/common/KeyNames.gen.hpp` (needs `pywin32`) |
| `tools/lang_json.py` | Convert between `res/langs/*.rc` and `res/langs/*.json` (`export`, `import`, `check`) |
| `tools/preflight.py` | Pre-commit gate: language tables, formatting, tests |

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
- A script whose delays are all zero runs as fast as the machine allows; the
  clicker can spin through thousands of rounds per second. Add an interval if
  that matters.
- Two instances share `%APPDATA%\KBMacro\config.json`; whichever closes last
  overwrites the window geometry remembered by the other.
- Closing the window while recording discards what has been recorded so far:
  no delivery dialog is shown and the actions are dropped.
- Global hotkeys belong to one instance: a second copy registers none of them
  (error 1409) and shows “Failed to register 11 hotkey(s)” in the status bar,
  while the first copy keeps working.
- When the foreground window runs at a higher integrity level (an elevated
  application), Windows discards injected input through UIPI while `SendInput`
  still reports success. Actions appear to execute in the list but have no
  effect on screen, and nothing is logged as an error.

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
