#pragma once

#include "common/Types.hpp"
#include <filesystem>
#include <string>

namespace utils {

// full path of the current EXE (empty path when unavailable)
std::filesystem::path GetExecutablePath();

// directory of the current EXE (falls back to current_path when unavailable)
std::filesystem::path GetExecutableDirectory();

// wide chars -> UTF-8
std::string ToUtf8(std::wstring_view wstr);

// UTF-8 -> wide chars
std::wstring ToWide(std::string_view str);

// file-name-friendly timestamp: 2026-09-15_22-30-15
std::wstring GetTimestampForFilename();

// primary screen spec (resolution + DPI).
// resolution comes from GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN),
// DPI from GetDeviceCaps(LOGPIXELSX), falling back to 96 on failure.
[[nodiscard]] ScreenSpec GetPrimaryScreenSpec();

} // namespace utils
