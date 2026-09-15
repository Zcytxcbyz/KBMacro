#pragma once

#include <filesystem>
#include <string>

namespace utils {

    // 当前 EXE 的完整路径（拿不到返回空 path）
    std::filesystem::path GetExecutablePath();

    // 当前 EXE 所在目录（拿不到则回退到 current_path）
    std::filesystem::path GetExecutableDirectory();

    // 宽字符 → UTF-8
    std::string ToUtf8(std::wstring_view wstr);

    // 文件名友好的时间戳：2026-09-15_22-30-15
    std::wstring GetTimestampForFilename();

} // namespace utils
