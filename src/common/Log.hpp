#pragma once

// ============================================================
// 日志门面
//
// ENABLE_LOGGING 未定义时：
//   - LOG_* 宏展开为空，参数不求值，零运行时开销
//   - Log::Init/Shutdown 是空操作
//
// ENABLE_LOGGING 定义时：
//   - 转发到 spdlog 的 SPDLOG_* 宏，保留源位置（%s:%#）
// ============================================================

#ifdef ENABLE_LOGGING

#include <spdlog/spdlog.h>

class Log {
public:
    Log() = delete;

    static void Init();
    static void Shutdown();
};

#define LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

#else  // ENABLE_LOGGING

// ReSharper disable once CppUseInternalLinkage
class [[maybe_unused]] Log {
public:
    Log() = delete;

    static void Init() {}
    static void Shutdown() {}
};

#define LOG_TRACE(...)    ((void)0)
#define LOG_DEBUG(...)    ((void)0)
#define LOG_INFO(...)     ((void)0)
#define LOG_WARN(...)     ((void)0)
#define LOG_ERROR(...)    ((void)0)
#define LOG_CRITICAL(...) ((void)0)

#endif  // ENABLE_LOGGING
