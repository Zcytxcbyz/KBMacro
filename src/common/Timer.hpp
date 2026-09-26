#pragma once

#include "common/ITimer.hpp"

// ============================================================
// ITimer 的两个实现
//
//   SleepTimer —— 分段睡眠
//       CPU 占用 ≈ 0；"等待中被停止"的响应延迟 = 一次 slice 对齐到系统节拍
//       （名义 kSlice = 10ms，实测约 15–25ms）；
//       但等待精度受系统定时器粒度影响（Windows 默认约 15.6ms），
//       因此普通模式下实际间隔可能略长于设定值——这是设计内的代价。
//
//   SpinTimer  —— 忙等
//       精度最好（受 steady_clock / QPC 限制）；停止响应亚微秒级；
//       代价是等待期间占满一个核心。这是明确的契约，
//       UI 侧"高精度模式"的 CPU 警告弹窗就是为它存在的。
//
// 两者都是无状态类：同一实例可被多个线程并发调用。
// 实现细节（deadline 循环、为什么不做累减）见 Timer.cpp。
// ============================================================

// 分段睡眠实现。
//
// 把一次长等待切成 <= kSlice 的小段，段间检查停止请求。
// 因此"等待中被停止"的最坏响应延迟 = 一个 slice 对齐到一次系统节拍
// （名义 10ms，实测约 15–25ms），并不等于名义上的 kSlice。
// 这个延迟同时也是 Sleep 模式下 Stop 阻塞 join 时 UI 线程的停顿上界。
class SleepTimer final : public ITimer {
public:
    // 分段粒度。对外暴露是为了让测试能断言停止延迟的上界。
    static constexpr Duration kSlice{10};

    [[nodiscard]] bool wait(Duration d, const std::stop_token &st) const override;
};

// 忙等实现。
//
// 循环里检查停止请求，相邻两次检查之间只隔一条 _mm_pause，
// 因此响应延迟是亚微秒级。
// 注意：等待期间 CPU 占用 100%（单核）——这是设计内的取舍，不是缺陷。
class SpinTimer final : public ITimer {
public:
    [[nodiscard]] bool wait(Duration d, const std::stop_token &st) const override;
};
