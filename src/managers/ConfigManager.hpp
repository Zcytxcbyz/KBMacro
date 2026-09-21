#pragma once

#include <filesystem>
#include <string_view>

#include "common/Config.hpp"
#include "common/Types.hpp"

// ============================================================
// 全局配置管理器
//
// 职责：
//   - 启动时判定配置路径（便携 / APPDATA）并加载
//   - 提供只读访问 config()
//   - 提供 setter，每个 setter 内部即改即存
//   - 便携模式的导出 / 删除
//   - 恢复默认值
//
// 不做：
//   - 变更通知（UI 是唯一读写方，无并发）
//   - reload()（语言等配置需重启生效）
//   - 各模块参数的持久化（模块自己走导入导出）
//
// 生命周期：
//   - 单例，在 main() 中 load() 一次
//   - 路径判定算法在 common/ConfigPaths 里，本类只做编排
// ============================================================

class ConfigManager {
public:
    static ConfigManager &instance();

    // 启动时调一次：判定路径 → 读文件 → 填 m_config；失败则保持默认值。
    // 读取失败不抛异常，行为等价于"用默认配置"。
    void load();

    // ---- 只读访问 ----

    [[nodiscard]] const Config &config() const noexcept { return m_config; }

    // 当前生效的配置文件路径
    [[nodiscard]] const std::filesystem::path &currentPath() const noexcept {
        return m_path;
    }

    // 是否便携模式（当前配置来自 EXE 同目录的 {exe}.json）
    [[nodiscard]] bool isPortable() const noexcept { return m_portable; }

    // ---- 修改（每个 setter 内部立即 save） ----

    void setLanguage(std::string_view code);
    void setTimerMode(TimerMode mode);
    void setAlwaysOnTop(bool on);
    void setHotkey(int index, const Hotkey &hk);
    void setSpinWarningConfirmed(bool v);

    // ---- 便携模式 ----

    // 把当前内存配置写到 {exe}.json，并切换到便携模式。
    // 之后所有 setter 的 save 都写这个文件。
    void exportToPortable();

    // 删除 {exe}.json，切回 APPDATA，并把当前内存配置写一份到 APPDATA。
    // 文件不存在时只做切换，不报错。
    void deletePortable();

    // ---- 重置 ----

    void clearSpinWarning();
    void resetToDefaults();

private:
    ConfigManager() = default;

    // 把 m_config 写到 m_path；失败只记日志，不抛。
    void save() const;

    // 从环境变量读 %APPDATA%
    [[nodiscard]] static std::filesystem::path appDataDir();

    Config m_config;
    std::filesystem::path m_path;
    bool m_portable = false;
};
