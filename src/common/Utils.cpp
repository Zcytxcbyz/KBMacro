#include "common/Utils.hpp"

#include <chrono>
#include <ctime>

#include <windows.h>
#include <wil/win32_helpers.h>

namespace utils {
    std::filesystem::path GetExecutablePath() {
        return {wil::GetModuleFileNameW().get()};
    }

    std::filesystem::path GetExecutableDirectory() {
        const auto path = GetExecutablePath();
        return path.empty() ? std::filesystem::current_path() : path.parent_path();
    }

    std::string ToUtf8(const std::wstring_view wstr) {
        if (wstr.empty()) return {};
        const int size = ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(),
                                               static_cast<int>(wstr.size()),
                                               nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<std::size_t>(size), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(),
                              static_cast<int>(wstr.size()),
                              result.data(), size, nullptr, nullptr);
        return result;
    }

    std::wstring ToWide(const std::string_view str) {
        if (str.empty()) return {};
        const int size = ::MultiByteToWideChar(CP_UTF8, 0, str.data(),
                                               static_cast<int>(str.size()),
                                               nullptr, 0);
        if (size <= 0) return {};
        std::wstring result(static_cast<std::size_t>(size), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, str.data(),
                              static_cast<int>(str.size()),
                              result.data(), size);
        return result;
    }

    std::wstring GetTimestampForFilename() {
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        ::localtime_s(&tm, &t);
        wchar_t buf[32] = {};
        std::wcsftime(buf, sizeof(buf) / sizeof(wchar_t),
                      L"%Y-%m-%d_%H-%M-%S", &tm);
        return buf;
    }
} // namespace utils
