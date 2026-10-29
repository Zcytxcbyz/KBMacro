#include "common/Timer.hpp"

#include <immintrin.h>   // _mm_pause()
#include <thread>        // std::this_thread::sleep_for

// ============================================================
// 两个实现都不打日志：wait 是热路径，按约定不记。
// （ENABLE_LOGGING 开关在这里无关紧要，因为压根没有 LOG_* 调用。）
// ============================================================

// ============================================================
// SleepTimer：分段睡眠
// ============================================================

bool SleepTimer::wait(const Duration d, const std::stop_token &st) const {
    if (d <= Duration::zero()) return !st.stop_requested();

    // 用绝对截止点重算剩余量，而不是对剩余量做累减：
    // sleep_for 的实际睡眠时间总是 >= 请求值，累减会把每次的超出量
    // 攒成总时长偏差（越睡越晚）。
    const auto deadline = now() + d;

    while (true) {
        // 先判停止：语义上"被打断"优先于"刚好到点"。
        if (st.stop_requested()) return false;
        if (now() >= deadline) return true;

        // ceil 而不是 duration_cast：Duration 是毫秒，直接截断会在
        // 最后一轮少算不到 1ms 的零头，导致提前返回（下限断言会挂）。
        // ceil 宁可多睡不到 1ms，相对系统定时器粒度完全可以忽略。
        const auto remaining = std::chrono::ceil<Duration>(deadline - now());

        std::this_thread::sleep_for(remaining < kSlice ? remaining : kSlice);
    }
}

// ============================================================
// SpinTimer：忙等
// ============================================================

bool SpinTimer::wait(const Duration d, const std::stop_token &st) const {
    if (d <= Duration::zero()) return !st.stop_requested();

    const auto deadline = now() + d;

    while (true) {
        if (st.stop_requested()) return false;
        if (now() >= deadline) return true;

        // 自旋提示：降低功耗、减少超线程下的执行单元争用。
        // 不加它纯 while 也能跑，但会无谓地打满译码/执行端口。
        _mm_pause();
    }
}

// ============================================================
// HybridTimer：长间隔分段睡、末尾忙等
// ============================================================
// 两段式：
//   1) 剩余 > spinTail → 分段 sleep（绝对 deadline 重算，与 SleepTimer 同法）
//   2) 剩余 <= spinTail → 忙等到 deadline
// 关键在第一步算的是"剩余 - spinTail"而不是"剩余"：否则最后一段 sleep 的
// 溢出会把尾巴吃掉，混合就白做了。
bool HybridTimer::wait(const Duration d, const std::stop_token &st) const {
    if (d <= Duration::zero()) return !st.stop_requested();

    const auto deadline = now() + d;

    // ---- 第一段：睡到 deadline - spinTail ----
    while (true) {
        if (st.stop_requested()) return false;

        const auto remaining = deadline - now();
        if (remaining <= Duration::zero()) return true;
        if (remaining <= m_spinTail) break;   // 进入尾巴，交给忙等

        const auto sleepFor = std::chrono::ceil<Duration>(remaining - m_spinTail);
        std::this_thread::sleep_for(sleepFor < SleepTimer::kSlice ? sleepFor : SleepTimer::kSlice);
    }

    // ---- 第二段：忙等到点 ----
    while (true) {
        if (st.stop_requested()) return false;
        if (now() >= deadline) return true;

        _mm_pause();
    }
}
