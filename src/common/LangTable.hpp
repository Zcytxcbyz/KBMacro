#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

// Parses one language table, i.e. {"1001": "KBMacro", "5001": "Interval (ms):"}.
//
// Keys are string ids written as text, values are the translated text. Anything
// that is not a well-formed table throws std::runtime_error, so the caller can
// fall back to the .rc string table instead of showing empty labels.
[[nodiscard]] std::unordered_map<int, std::wstring> parseLangTable(std::string_view utf8Json);
