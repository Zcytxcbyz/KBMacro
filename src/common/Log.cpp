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
#include <spdlog/sinks/msvc_sink.h>

#include "common/Utils.hpp"

namespace {
    constexpr auto kLoggerName = "KBMacro";
    constexpr auto kLogPattern = "[%Y-%m-%d %H:%M:%S.%e] [%t] [%s:%#] [%l] %v";
#ifdef KBMACRO_DRY_RUN
    // 干跑时动作密度可能远高于真实运行（回放 delayMs 很小的脚本），
    // 8192 条会在 overrun_oldest 策略下悄悄丢最旧的行。给足余量。
    constexpr std::size_t kAsyncQueueSize = 65536;
#else
    constexpr std::size_t kAsyncQueueSize = 8192;
#endif
    constexpr std::size_t kMaxLogFiles = 30;

    std::filesystem::path GetLogDirectory() {
        return utils::GetExecutableDirectory() / L"logs";
    }

    // 保留最近 keepCount 个日志文件，删掉更旧的
    void CleanOldLogs(const std::filesystem::path &logDir, const std::size_t keepCount) {
        std::vector<std::filesystem::path> logFiles;

        std::error_code ec;
        const auto it = std::filesystem::directory_iterator(logDir, ec);
        if (ec) return;

        for (const auto &entry : it) {
            if (std::error_code entryEc;
                entry.is_regular_file(entryEc) && entry.path().extension() == L".log") {
                logFiles.push_back(entry.path());
            }
        }

        if (logFiles.size() <= keepCount) return;

        // 文件名格式 KBMacro_YYYY-MM-DD_HH-MM-SS.log
        // 字典序 == 时间序，直接按路径排
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

        const auto logFile =
            logDir / (L"KBMacro_" + utils::GetTimestampForFilename() + L".log");

        spdlog::init_thread_pool(kAsyncQueueSize, 1);

        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            utils::ToUtf8(logFile.native()), /*truncate=*/true);

        // 同时送一份到调试器输出（VS 输出窗口）。sink 默认 check_debugger_present_ = true，
        // 没挂调试器时每行只做一次 IsDebuggerPresent() 就返回，不格式化也不写。
        auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

        auto logger = std::make_shared<spdlog::async_logger>(
            kLoggerName,
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

#endif  // ENABLE_LOGGING
