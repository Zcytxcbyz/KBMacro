#include "common/ConfigPaths.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

#include "common/Log.hpp"
#include "common/Utils.hpp"

// ============================================================
// path resolution
// ============================================================

namespace config {

ResolvedPath resolveConfigPath(const std::filesystem::path &exePath, const std::filesystem::path &appDataDir) {
    // portable mode: {exe}.json next to the EXE (e.g. KBMacro.exe.json)
    // naming rule matches the existing SettingsPanel implementation
    std::filesystem::path portable = exePath;
    portable.concat(L".json");

    if (std::error_code ec; std::filesystem::exists(portable, ec) && !ec) {
        return {.path = std::move(portable), .portable = true};
    }

    // APPDATA mode: {appDataDir}/KBMacro/config.json
    const auto regular = appDataDir / L"KBMacro" / L"config.json";
    return {.path = regular, .portable = false};
}

// ============================================================
// load
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

    // version check: file version above the supported one -> refuse to load
    // lower or equal -> read as the current version (missing fields use defaults)
    // the read itself is guarded: a hand-edited file may hold a string where an integer belongs
    int fileVersion = 1;
    try {
        fileVersion = j.value("version", 1);
    } catch (const std::exception &e) {
        LOG_WARN("config: bad version field: {} ({})", pathStr, e.what());
        return false;
    }
    if (fileVersion > kConfigVersion) {
        LOG_WARN("config: file version {} > supported {}, ignoring", fileVersion, kConfigVersion);
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
// save
// ============================================================

bool saveTo(const std::filesystem::path &path, const Config &cfg) {
    const std::string pathStr = utils::ToUtf8(path.wstring());

    // create the parent directory when missing
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            LOG_ERROR("config: cannot create directory: {} ({})", utils::ToUtf8(parent.wstring()), ec.message());
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
