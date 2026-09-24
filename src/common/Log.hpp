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

// spdlog 默认 SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_INFO，
// 会把 TRACE / DEBUG 在编译期直接剪掉，运行期无论 logger->set_level
// 设成什么都不会输出。这里显式放开到 TRACE，让运行期级别
// （Log.cpp 里 logger->set_level(debug)）成为唯一过滤器。
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

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
class Log {
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
