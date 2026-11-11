#include "common/Log.hpp"

#ifdef ENABLE_LOGGING

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include "common/Utils.hpp"

namespace {
constexpr auto kLoggerName = "KBMacro";
constexpr auto kLogPattern = "[%Y-%m-%d %H:%M:%S.%e] [%t] [%s:%#] [%l] %v";
#ifdef KBMACRO_DRY_RUN
// a dry run can have a much higher action density than a real run (playing a script with tiny delayMs),
// 8192 entries would silently drop the oldest lines under overrun_oldest. leave plenty of headroom.
constexpr std::size_t kAsyncQueueSize = 65536;
#else
constexpr std::size_t kAsyncQueueSize = 8192;
#endif
constexpr std::size_t kMaxLogFiles = 30;

std::filesystem::path GetLogDirectory() {
    return utils::GetExecutableDirectory() / L"logs";
}

// keep the newest keepCount log files and delete the older ones
void CleanOldLogs(const std::filesystem::path &logDir, const std::size_t keepCount) {
    std::vector<std::filesystem::path> logFiles;

    std::error_code ec;
    const auto it = std::filesystem::directory_iterator(logDir, ec);
    if (ec) return;

    for (const auto &entry : it) {
        if (std::error_code entryEc; entry.is_regular_file(entryEc) && entry.path().extension() == L".log") {
            logFiles.push_back(entry.path());
        }
    }

    if (logFiles.size() <= keepCount) return;

    // file name format KBMacro_YYYY-MM-DD_HH-MM-SS.log
    // lexicographic order == chronological order, so sorting by path is enough
    std::ranges::sort(logFiles);

    const std::size_t deleteCount = logFiles.size() - keepCount;
    for (std::size_t i = 0; i < deleteCount; ++i) {
        std::error_code removeEc;
        std::filesystem::remove(logFiles[i], removeEc);
    }
}
} // namespace

void Log::Init() {
    try {
        const auto logDir = GetLogDirectory();
        std::filesystem::create_directories(logDir);

        const auto logFile = logDir / (L"KBMacro_" + utils::GetTimestampForFilename() + L".log");

        spdlog::init_thread_pool(kAsyncQueueSize, 1);

        auto fileSink =
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(utils::ToUtf8(logFile.native()), /*truncate=*/true);

        // mirror to the debugger output too (VS output window). check_debugger_present_ = true by default,
        // without a debugger each line only does one IsDebuggerPresent() and returns, no formatting or writing.
        auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

        auto logger = std::make_shared<spdlog::async_logger>(kLoggerName,
                                                             spdlog::sinks_init_list{fileSink, msvcSink},
                                                             spdlog::thread_pool(),
                                                             spdlog::async_overflow_policy::overrun_oldest);

        logger->set_pattern(kLogPattern);
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::warn);

        spdlog::set_default_logger(logger);
        spdlog::flush_every(std::chrono::seconds(3));
        CleanOldLogs(logDir, kMaxLogFiles);
    } catch (const std::exception &e) {
        ::OutputDebugStringA("Log init failed: ");
        ::OutputDebugStringA(e.what());
        ::OutputDebugStringA("\n");
    }
}

void Log::Shutdown() {
    spdlog::shutdown();
}

#endif // ENABLE_LOGGING
