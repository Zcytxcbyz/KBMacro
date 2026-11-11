#pragma once

#include <filesystem>

#include <nlohmann/json.hpp>

// ============================================================
// JSON file read and write (pure functions)
//
// the generic part pulled out of ConfigPaths, for "parameter import/export" scenarios:
// the caller builds/parses the json object; this module only does file IO.
//
// differences from ConfigPaths:
//   - ConfigPaths does version checks and portable/APPDATA path resolution
//   - JsonIO only does "write json to a file / read json from a file"
//   - the version wrapper is up to the caller (parameter files use version too, handled above)
// ============================================================

namespace jsonio {

// write json to a file.
//   - the parent directory is created when missing
//   - 2-space indent, trailing newline
//   returns true  = success
//   returns false = failure (details in LOG_ERROR)
[[nodiscard]] bool saveTo(const std::filesystem::path &path, const nlohmann::json &j);

// read json from a file.
//   returns true  = success
//   returns false = failure (missing file / open failure / parse failure), out keeps its defaults
//
// failure reasons are logged as LOG_INFO (missing file) / LOG_WARN (open and parse failures)
[[nodiscard]] bool loadFrom(const std::filesystem::path &path, nlohmann::json &out);

} // namespace jsonio
