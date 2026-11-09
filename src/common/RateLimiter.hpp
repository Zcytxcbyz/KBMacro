#pragma once

#include "common/ITimer.hpp"

// 限频开关：每隔一段间隔才放行一次。
//
// 用途：把高频事件的日志/汇报压到可读量级（例如录制进度每秒一条）。
// 单独成类的原因：这属于通用小工具，多处都要用；复制"记时间 + 比间隔"
//   很容易把边界写错（首次要不要立即放行、等于间隔算不算到）。
//
// 契约：
//   - 纯逻辑：不打日志、不读配置、不持线程；时间由调用方给，便于测试注入
//   - 首次调用立即放行（不做"第一秒空白"）
//   - 判定是"距上次放行 >= interval"，不是固定节拍（不累积、不补偿）
class RateLimiter final {
public:
    explicit RateLimiter(const ITimer::Duration interval) noexcept : m_interval(interval) {}

    // 用当前 steady_clock 判断（不是系统调用）
    [[nodiscard]] bool allow() noexcept { return allow(ITimer::now()); }

    // 用给定时间点判断（测试注入）
    [[nodiscard]] bool allow(const ITimer::TimePoint now) noexcept {
        if (!m_started) {
            m_started = true;
            m_last = now;
            return true;
        }
        if (now - m_last < m_interval) return false;
        m_last = now;
        return true;
    }

    void reset() noexcept { m_started = false; }

    [[nodiscard]] ITimer::Duration interval() const noexcept { return m_interval; }

private:
    ITimer::Duration m_interval{};
    ITimer::TimePoint m_last{};
    bool m_started = false;
};
