#pragma once

// ============================================================
// logging facade
//
// when ENABLE_LOGGING is not defined:
//   - LOG_* macros expand to nothing, arguments are not evaluated, zero runtime cost
//   - Log::Init/Shutdown are no-ops
//
// when ENABLE_LOGGING is defined:
//   - forwards to spdlog's SPDLOG_* macros, keeping the source location (%s:%#)
// ============================================================

#ifdef ENABLE_LOGGING

// spdlog defaults SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_INFO,
// which strips TRACE / DEBUG at compile time, so whatever logger->set_level
// is set to at runtime, nothing is emitted. this file explicitly opens it up to TRACE so the runtime level
// (logger->set_level(debug) in Log.cpp) becomes the only filter.
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
