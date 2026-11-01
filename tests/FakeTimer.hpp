#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "common/ITimer.hpp"

// ============================================================
// 测试用计时器：不睡觉，只记录调用、按脚本返回。
//
// 注意一：ITimer::now() 是静态函数，被测模块直接调它，FakeTimer 控制不了
//         时间流动——它只能让 wait 立刻返回。因此 Timed 模式的轮数不在
//         这里测（那要真等满 durationSec），交给 RunController 的用例。
// 注意二：input::click()/pressKey() 内部的"按住等待"也走这个 ITimer，
//         所以 waited() 里混着 holdMs 和轮间隔两种时长。断言时用
//         countWaits(时长) 分别数，不要拿总次数去推轮数。
// ============================================================
class FakeTimer final : public ITimer {
public:
    // stopAtCall：第 N 次 wait（从 1 起，按住与间隔都算）返回 false。
    //             <= 0 表示永不打断。
    explicit FakeTimer(const int stopAtCall = 0) noexcept : m_stopAtCall(stopAtCall) {}

    [[nodiscard]] bool wait(const Duration d, const std::stop_token &st) const override {
        ++m_waitCalls;
        m_waited.push_back(d);

        if (st.stop_requested()) return false;
        if (m_stopAtCall > 0 && m_waitCalls >= m_stopAtCall) return false;
        return true;
    }

    [[nodiscard]] int waitCalls() const noexcept { return m_waitCalls; }

    [[nodiscard]] const std::vector<Duration> &waited() const noexcept { return m_waited; }

    // 指定时长的等待出现了几次（把"按住等待"和"轮间隔"分开数的唯一办法）
    [[nodiscard]] std::size_t countWaits(const Duration d) const {
        return static_cast<std::size_t>(std::count(m_waited.begin(), m_waited.end(), d));
    }

private:
    int m_stopAtCall = 0;
    mutable int m_waitCalls = 0;
    mutable std::vector<Duration> m_waited;
};
