#include "common/ConfigPaths.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

#include "common/Log.hpp"
#include "common/Utils.hpp"

// ============================================================
// 路径判定
// ============================================================

namespace config {

ResolvedPath resolveConfigPath(const std::filesystem::path &exePath,
                               const std::filesystem::path &appDataDir) {
    // 便携模式：EXE 同目录 {exe}.json（如 KBMacro.exe.json）
    // 命名规则和 SettingsPanel 现有实现一致
    std::filesystem::path portable = exePath;
    portable.concat(L".json");

    if (std::error_code ec; std::filesystem::exists(portable, ec) && !ec) {
        return {.path = std::move(portable), .portable = true};
    }

    // APPDATA 模式：{appDataDir}/KBMacro/config.json
    const auto regular = appDataDir / L"KBMacro" / L"config.json";
    return {.path = regular, .portable = false};
}

// ============================================================
// 读取
// ============================================================

bool loadFrom(const std::filesystem::path &path, Config &out) {
    const std::string pathStr = utils::ToUtf8(path.wstring());

    if (std::error_code ec; !std::filesystem::exists(path, ec) || ec) {
        LOG_INFO("config: file not found: {}", pathStr);
        return false;
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        LOG_WARN("config: cannot open file: {}", pathStr);
        return false;
    }

    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception &e) {
        LOG_WARN("config: parse failed: {} ({})", pathStr, e.what());
        return false;
    }

    // 版本校验：文件版本高于当前程序支持的版本 → 拒绝加载
    // 低于或等于当前版本 → 按当前版本读取（缺字段走默认）
    if (const int fileVersion = j.value("version", 1); fileVersion > kConfigVersion) {
        LOG_WARN("config: file version {} > supported {}, ignoring",
                 fileVersion, kConfigVersion);
        return false;
    }

    try {
        out = j.get<Config>();
    } catch (const std::exception &e) {
        LOG_WARN("config: deserialize failed: {} ({})", pathStr, e.what());
        return false;
    }

    LOG_INFO("config: loaded from {}", pathStr);
    return true;
}

// ============================================================
// 写入
// ============================================================

bool saveTo(const std::filesystem::path &path, const Config &cfg) {
    const std::string pathStr = utils::ToUtf8(path.wstring());

    // 父目录不存在时自动创建
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            LOG_ERROR("config: cannot create directory: {} ({})",
                      utils::ToUtf8(parent.wstring()), ec.message());
            return false;
        }
    }

    nlohmann::json j = cfg;

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        LOG_ERROR("config: cannot open file for write: {}", pathStr);
        return false;
    }

    try {
        ofs << j.dump(2) << '\n';
    } catch (const std::exception &e) {
        LOG_ERROR("config: write failed: {} ({})", pathStr, e.what());
        return false;
    }

    if (!ofs) {
        LOG_ERROR("config: write stream error: {}", pathStr);
        return false;
    }

    LOG_DEBUG("config: saved to {}", pathStr);
    return true;
}

} // namespace config
