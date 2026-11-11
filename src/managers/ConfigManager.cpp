#include "managers/ConfigManager.hpp"

#include <string>
#include <algorithm>

#include "common/ConfigPaths.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"

// ============================================================
// Singleton
// ============================================================

ConfigManager &ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

// ============================================================
// %APPDATA% directory
// ============================================================

std::filesystem::path ConfigManager::appDataDir() {
    // the environment variable is the simplest way. %APPDATA% is always set in a normal Windows session,
    // in the rare missing case (service process etc.) fall back to the EXE directory, avoiding the CWD.
    if (const wchar_t *appData = _wgetenv(L"APPDATA"); appData && *appData) {
        return {appData};
    }
    LOG_WARN("config: %APPDATA% not set, falling back to exe directory");
    return utils::GetExecutableDirectory();
}

// ============================================================
// Load
// ============================================================

void ConfigManager::load() {
    const auto exePath = utils::GetExecutablePath();
    const auto appData = appDataDir();

    const auto [path, portable] = config::resolveConfigPath(exePath, appData);
    m_path = path;
    m_portable = portable;

    LOG_INFO("config: resolved path: {} (portable={})",
             utils::ToUtf8(m_path.wstring()), m_portable);

    if (!config::loadFrom(m_path, m_config)) {
        LOG_INFO("config: initializing new config file");
        save();
    }
}

// ============================================================
// Save (called inside setters)
// ============================================================

void ConfigManager::save() const {
    (void)config::saveTo(m_path, m_config);
}

// ============================================================
// Generic setters
// ============================================================

void ConfigManager::setLanguage(const std::string_view code) {
    if (m_config.language == code) return;
    m_config.language = std::string(code);
    save();
    LOG_DEBUG("config: language set to '{}'", code);
}

void ConfigManager::setTimerMode(const TimerMode mode) {
    if (m_config.timerMode == mode) return;
    m_config.timerMode = mode;
    save();
    LOG_DEBUG("config: timerMode set to '{}'", timerModeToString(mode));
}

void ConfigManager::setTimerHybridSpinMs(const int ms) {
    const int clamped = std::clamp(ms, kTimerHybridSpinMsMin, kTimerHybridSpinMsMax);
    if (m_config.timerHybridSpinMs == clamped) return;
    m_config.timerHybridSpinMs = clamped;
    save();
    LOG_DEBUG("config: timerHybridSpinMs set to {}ms", clamped);
}

void ConfigManager::setRecordMoveIntervalMs(const int ms) {
    const int clamped = std::clamp(ms, kRecordMoveIntervalMsMin, kRecordMoveIntervalMsMax);
    if (m_config.recordMoveIntervalMs == clamped) return;
    m_config.recordMoveIntervalMs = clamped;
    save();
    LOG_DEBUG("config: recordMoveIntervalMs set to {}ms", clamped);
}

void ConfigManager::setRecordIgnoreInjected(const bool v) {
    if (m_config.recordIgnoreInjected == v) return;
    m_config.recordIgnoreInjected = v;
    save();
    LOG_DEBUG("config: recordIgnoreInjected set to {}", v);
}

void ConfigManager::setRecordStartDelayMs(const int ms) {
    const int clamped = std::clamp(ms, kRecordStartDelayMsMin, kRecordStartDelayMsMax);
    if (m_config.recordStartDelayMs == clamped) return;
    m_config.recordStartDelayMs = clamped;
    save();
    LOG_DEBUG("config: recordStartDelayMs set to {}ms", clamped);
}

void ConfigManager::setRecordMaxActions(const int n) {
    const int clamped = std::clamp(n, kRecordMaxActionsMin, kRecordMaxActionsMax);
    if (m_config.recordMaxActions == clamped) return;
    m_config.recordMaxActions = clamped;
    save();
    LOG_DEBUG("config: recordMaxActions set to {}", clamped);
}

void ConfigManager::setPlaybackSpeed(const double speed) {
    const double snapped = snapPlaybackSpeed(speed);
    if (snapped == m_config.playbackSpeed) {
        return;
    }

    m_config.playbackSpeed = snapped;
    save();
    LOG_DEBUG("config: playback speed set to {}x", snapped);
}

void ConfigManager::setAlwaysOnTop(const bool on) {
    if (m_config.alwaysOnTop == on) return;
    m_config.alwaysOnTop = on;
    save();
    LOG_DEBUG("config: alwaysOnTop set to {}", on);
}

void ConfigManager::setHotkeyEnabled(const int index, const bool enabled) {
    if (index < 0 || index >= kHotkeyCount) {
        LOG_WARN("config: setHotkeyEnabled index out of range: {}", index);
        return;
    }

    const auto i = static_cast<std::size_t>(index);
    if (m_config.hotkeyEnabled[i] == enabled) return;

    m_config.hotkeyEnabled[i] = enabled;
    save();
    LOG_DEBUG("config: hotkey[{}] enabled set to {}", index, enabled);
}

void ConfigManager::setHotkey(const int index, const Hotkey &hk) {
    if (index < 0 || index >= kHotkeyCount) {
        LOG_WARN("config: setHotkey index out of range: {}", index);
        return;
    }
    m_config.hotkeys[static_cast<std::size_t>(index)] = hk;
    save();
    LOG_DEBUG("config: hotkey[{}] updated", index);
}

void ConfigManager::setSpinWarningConfirmed(const bool v) {
    if (m_config.flags.spinWarningConfirmed == v) return;
    m_config.flags.spinWarningConfirmed = v;
    save();
    LOG_DEBUG("config: spinWarningConfirmed set to {}", v);
}

void ConfigManager::setAdaptationPreference(const AdaptationPreference pref) {
    if (m_config.flags.adaptationPreference == pref) return;
    m_config.flags.adaptationPreference = pref;
    save();
    LOG_DEBUG("config: adaptationPreference set to {}",
              adaptationPreferenceToString(pref));
}

void ConfigManager::setWindow(const Config::Window &w) {
    // ReSharper disable once CppUseStructuredBinding
    // ReSharper disable once CppTooWideScopeInitStatement
    const auto &old = m_config.window;
    if (old.x == w.x && old.y == w.y &&
        old.width == w.width && old.height == w.height &&
        old.maximized == w.maximized) {
        return;
        }
    m_config.window = w;
    save();
    LOG_DEBUG("config: window set to ({},{}) {}x{} max={}",
              w.x, w.y, w.width, w.height, w.maximized);
}

void ConfigManager::setTabIndex(const int index) {
    if (m_config.tabIndex == index) return;
    m_config.tabIndex = index;
    save();
    LOG_DEBUG("config: tabIndex set to {}", index);
}

// ============================================================
// Portable mode
// ============================================================

bool ConfigManager::exportToPortable() {
    const auto exePath = utils::GetExecutablePath();
    auto portablePath = exePath;
    portablePath.concat(L".json");

    if (!config::saveTo(portablePath, m_config)) {
        // file write failed: state unchanged, still the original APPDATA mode
        LOG_ERROR("config: export to portable failed, state unchanged");
        return false;
    }

    m_path = std::move(portablePath);
    m_portable = true;
    LOG_INFO("config: switched to portable mode: {}",
             utils::ToUtf8(m_path.wstring()));
    return true;
}

bool ConfigManager::deletePortable() {
    // idempotent: already not in portable mode, treat as success
    if (!m_portable) {
        LOG_DEBUG("config: deletePortable called but not in portable mode");
        return true;
    }

    // remove the portable file
    std::error_code ec;
    const bool removed = std::filesystem::remove(m_path, ec);

    if (ec) {
        // delete failed: state unchanged, still portable mode
        LOG_ERROR("config: remove portable failed: {} ({}), state unchanged",
                  utils::ToUtf8(m_path.wstring()), ec.message());
        return false;
    }

    if (removed) {
        LOG_INFO("config: portable config removed: {}",
                 utils::ToUtf8(m_path.wstring()));
    } else {
        // the file was already absent (deleted by hand): not an error, keep switching
        LOG_DEBUG("config: portable file already absent: {}",
                  utils::ToUtf8(m_path.wstring()));
    }

    // switch back to APPDATA
    const auto [path, portable] = config::resolveConfigPath(
        utils::GetExecutablePath(), appDataDir());
    m_path = path;
    m_portable = false;

    // write the in-memory config to APPDATA so the next start does not "revert to ancient state"
    save();
    LOG_INFO("config: switched back to APPDATA: {}",
             utils::ToUtf8(m_path.wstring()));
    return true;
}

// ============================================================
// Reset
// ============================================================

void ConfigManager::clearSpinWarning() {
    if (!m_config.flags.spinWarningConfirmed) return;
    m_config.flags.spinWarningConfirmed = false;
    save();
    LOG_DEBUG("config: spinWarningConfirmed cleared");
}

void ConfigManager::resetToDefaults() {
    m_config = Config{};
    save();
    LOG_INFO("config: reset to defaults");
}
