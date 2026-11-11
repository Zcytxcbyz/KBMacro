#pragma once

#include <filesystem>
#include <string_view>

#include "common/Config.hpp"
#include "common/Types.hpp"

// ============================================================
// Global config manager
//
// Responsibilities:
//   - decide the config path at startup (portable / APPDATA) and load
//   - provide read-only access via config()
//   - provide setters, each setter saves immediately
//   - export / delete for portable mode
//   - restore defaults
//
// Not responsible for:
//   - change notification (the UI is the only reader/writer, no concurrency)
//   - reload() (language and other configs need a restart to take effect)
//   - persisting per-module params (modules use their own import/export)
//
// Lifetime:
//   - singleton, load() once in main()
//   - the path decision algorithm lives in common/ConfigPaths, this class only orchestrates
// ============================================================

class ConfigManager {
public:
    static ConfigManager &instance();

    // call once at startup: decide path → read file → fill m_config; on failure keep the defaults.
    // a read failure does not throw, equivalent to "use the default config".
    void load();

    // ---- read-only access ----

    [[nodiscard]] const Config &config() const noexcept { return m_config; }

    // currently effective config file path
    [[nodiscard]] const std::filesystem::path &currentPath() const noexcept { return m_path; }

    // whether portable mode (current config comes from {exe}.json next to the EXE)
    [[nodiscard]] bool isPortable() const noexcept { return m_portable; }

    // ---- modification (each setter saves immediately) ----

    void setLanguage(std::string_view code);
    void setTimerMode(TimerMode mode);
    // trailing busy-wait duration of the hybrid timer (ms), out-of-range values are clamped to
    // kTimerHybridSpinMsMin/Max
    void setTimerHybridSpinMs(int ms);
    // recording settings, out-of-range values are clamped to the matching kRecord*Min/Max
    void setRecordMoveIntervalMs(int ms);
    void setRecordIgnoreInjected(bool v);
    void setRecordStartDelayMs(int ms);
    void setRecordMaxActions(int n);
    // playback factor: snapped to the nearest step, then persisted
    void setPlaybackSpeed(double speed);
    void setAlwaysOnTop(bool on);
    void setHotkey(int index, const Hotkey &hk);
    void setHotkeyEnabled(int index, bool enabled);
    void setSpinWarningConfirmed(bool v);
    void setAdaptationPreference(AdaptationPreference pref);
    void setWindow(const Config::Window &w);
    void setTabIndex(int index);

    // ---- portable mode ----

    // write the current in-memory config to {exe}.json and switch to portable mode.
    // every later setter save writes this file.
    //   returns true  = success
    //   returns false = file write failed, state unchanged (still the original APPDATA mode)
    [[nodiscard]] bool exportToPortable();

    // delete {exe}.json, switch back to APPDATA, and write a copy of the in-memory config to APPDATA.
    //   returns true  = success (including the idempotent case "was not portable anyway")
    //   returns false = file delete failed, state unchanged (still portable mode)
    [[nodiscard]] bool deletePortable();

    // ---- reset ----

    void clearSpinWarning();
    void resetToDefaults();

private:
    ConfigManager() = default;

    // write m_config to m_path; on failure only log, do not throw.
    void save() const;

    // read %APPDATA% from the environment
    [[nodiscard]] static std::filesystem::path appDataDir();

    Config m_config;
    std::filesystem::path m_path;
    bool m_portable = false;
};
