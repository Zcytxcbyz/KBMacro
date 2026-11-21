#pragma once

#include <filesystem>

#include "common/Config.hpp"

// ============================================================
// config file path resolution, read and write (pure functions)
//
// why it was pulled out of ConfigManager:
//   1. the path resolution algorithm needs unit tests (P1); a singleton makes fake paths hard to inject
//   2. IO logic and lifecycle management (load once, setters write immediately) are two concerns
//
// pure functions have no global state; all parameters are passed explicitly, so tests can feed fake paths.
// the real path comes from ConfigManager.
// ============================================================

namespace config {

// config path resolution result
struct ResolvedPath {
    std::filesystem::path path;
    bool portable = false; // true = portable mode ({exe}.json next to the EXE)
};

// resolve the config path (pure function, no IO)
//   {exePath}.json exists -> portable mode, use that file
//   otherwise             -> APPDATA mode,
//                              {appDataDir}/KBMacro/config.json
//
// note: portable is decided by "does the file exist", not "is it writable".
// an existing but non-writable file is a manual user error; load/save will fail then,
// and the caller decides what to do next.
[[nodiscard]] ResolvedPath resolveConfigPath(const std::filesystem::path &exePath,
                                             const std::filesystem::path &appDataDir);

// read the config from a file.
//   returns true  = loaded successfully
//   returns false = not loaded (any of: missing file / parse failure / version too high),
//                out keeps its defaults
//
// failure reasons are logged as LOG_INFO (missing file) / LOG_WARN (parse failure, version too high).
[[nodiscard]] bool loadFrom(const std::filesystem::path &path, Config &out);

// write the config to a file; the parent directory is created when missing.
//   returns true  = written successfully
//   returns false = write failed (details in LOG_ERROR)
[[nodiscard]] bool saveTo(const std::filesystem::path &path, const Config &cfg);

} // namespace config
