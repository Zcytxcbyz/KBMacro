#include "common/Log.hpp"

#ifdef ENABLE_LOGGING

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <algorithm>
#include <vector>


#include <windows.h>

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "common/Utils.hpp"

namespace {
    constexpr const char *kLoggerName = "KBMacro";
    constexpr const char *kLogPattern = "[%Y-%m-%d %H:%M:%S.%e] [%t] [%s:%#] [%l] %v";
    constexpr std::size_t kAsyncQueueSize = 8192;
    constexpr std::size_t kMaxLogFiles = 30;

    std::filesystem::path GetLogDirectory() {
        return utils::GetExecutableDirectory() / L"logs";
    }

    // 保留最近 keepCount 个日志文件，删掉更旧的
    void CleanOldLogs(const std::filesystem::path &logDir, std::size_t keepCount) {
        std::vector<std::filesystem::path> logFiles;

        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator(logDir, ec)) {
            if (ec) break;
            if (entry.is_regular_file() && entry.path().extension() == L".log") {
                logFiles.push_back(entry.path());
            }
        }

        if (logFiles.size() <= keepCount) return;

        // 文件名格式 KBMacro_YYYY-MM-DD_HH-MM-SS.log
        // 字典序 == 时间序，直接按路径排
        std::sort(logFiles.begin(), logFiles.end());

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

        const auto logFile =
            logDir / (L"KBMacro_" + utils::GetTimestampForFilename() + L".log");

        spdlog::init_thread_pool(kAsyncQueueSize, 1);

        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            utils::ToUtf8(logFile.native()), /*truncate=*/true);

        auto logger = std::make_shared<spdlog::async_logger>(
            kLoggerName,
            fileSink,
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

#endif  // ENABLE_LOGGING
