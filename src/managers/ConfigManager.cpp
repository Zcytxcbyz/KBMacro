#include "managers/ConfigManager.hpp"

#include <string>

#include "common/ConfigPaths.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"

// ============================================================
// 单例
// ============================================================

ConfigManager &ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

// ============================================================
// %APPDATA% 目录
// ============================================================

std::filesystem::path ConfigManager::appDataDir() {
    // 环境变量方式最简单。%APPDATA% 在正常 Windows 会话下必有值，
    // 缺失的极端场景（服务进程等）回退到 EXE 目录，避免落到 CWD。
    if (const wchar_t *appData = _wgetenv(L"APPDATA"); appData && *appData) {
        return {appData};
    }
    LOG_WARN("config: %APPDATA% not set, falling back to exe directory");
    return utils::GetExecutableDirectory();
}

// ============================================================
// 加载
// ============================================================

void ConfigManager::load() {
    const auto exePath = utils::GetExecutablePath();
    const auto appData = appDataDir();

    const auto [path, portable] = config::resolveConfigPath(exePath, appData);
    m_path = path;
    m_portable = portable;

    LOG_INFO("config: resolved path: {} (portable={})",
             utils::ToUtf8(m_path.wstring()), m_portable);

    // 加载失败不报错：全新环境没文件是正常的，行为等价于默认配置。
    (void)config::loadFrom(m_path, m_config);
}

// ============================================================
// 保存（setter 内部调用）
// ============================================================

void ConfigManager::save() const {
    (void)config::saveTo(m_path, m_config);
}

// ============================================================
// 通用 setter
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

void ConfigManager::setAlwaysOnTop(const bool on) {
    if (m_config.alwaysOnTop == on) return;
    m_config.alwaysOnTop = on;
    save();
    LOG_DEBUG("config: alwaysOnTop set to {}", on);
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

// ============================================================
// 便携模式
// ============================================================

void ConfigManager::exportToPortable() {
    const auto exePath = utils::GetExecutablePath();
    auto portablePath = exePath;
    portablePath.concat(L".json");

    if (!config::saveTo(portablePath, m_config)) {
        LOG_ERROR("config: export to portable failed");
        return;
    }

    m_path = std::move(portablePath);
    m_portable = true;
    LOG_INFO("config: switched to portable mode: {}",
             utils::ToUtf8(m_path.wstring()));
}

void ConfigManager::deletePortable() {
    if (!m_portable) {
        LOG_DEBUG("config: deletePortable called but not in portable mode");
        return;
    }

    // 删除便携文件（文件可能已被用户手动删掉，不视为错误）
    std::error_code ec;
    const bool removed = std::filesystem::remove(m_path, ec);
    if (ec) {
        LOG_WARN("config: remove portable failed: {} ({})",
                 utils::ToUtf8(m_path.wstring()), ec.message());
    } else if (removed) {
        LOG_INFO("config: portable config removed: {}",
                 utils::ToUtf8(m_path.wstring()));
    }

    // 切回 APPDATA
    const auto [path, portable] = config::resolveConfigPath(
        utils::GetExecutablePath(), appDataDir());
    m_path = path;
    m_portable = false;

    // 把当前内存配置写到 APPDATA，避免下次启动时"回到远古状态"
    save();
    LOG_INFO("config: switched back to APPDATA: {}",
             utils::ToUtf8(m_path.wstring()));
}

// ============================================================
// 重置
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
