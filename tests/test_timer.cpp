// ============================================================
// ITimer 实现测试（SleepTimer / SpinTimer / HybridTimer）
//
// 计时断言的写法规矩：
//   - 下限断言（"不早于 X"）才有意义，必须严格——它保证不会提前返回
//   - 上限断言给足裕量：Windows 默认定时器粒度约 15.6ms，sleep_for 会睡过头；
//     机器负载抖动也会拉长实测值。上界卡太紧只会得到 flaky 用例
//
// 两者共有的语义用 TYPED_TEST 跑两遍，实现特有的（延迟上界、精度）单独写。
// ============================================================

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stop_token>
#include <thread>

#include "common/Timer.hpp"

namespace {

    using Clock = ITimer::Clock;
    using Duration = ITimer::Duration;

    // 实测一段代码的耗时（毫秒）
    template <typename Fn>
    long long measureMs(Fn &&fn) {
        const auto t0 = Clock::now();
        fn();
        return std::chrono::duration_cast<Duration>(Clock::now() - t0).count();
    }

    // 让 timer 进入等待，30ms 后请求停止，返回"请求停止 → wait 返回"的实测延迟。
    // 返回 -1 表示 wait 没有被 stop 打断（等满了），用例据此判失败。
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    long long stopLatencyMs(ITimer &timer, const Duration d) {
        std::stop_source src;
        bool interrupted = false;

        std::jthread worker([&] {
            interrupted = !timer.wait(d, src.get_token());   // wait 返回 false = 被打断
        });

        // 先让 worker 真正进到等待里，再请求停止
        std::this_thread::sleep_for(Duration(30));

        const auto t0 = Clock::now();
        src.request_stop();
        worker.join();          // join 返回 ≈ wait 返回

        if (!interrupted) return -1;
        return std::chrono::duration_cast<Duration>(Clock::now() - t0).count();
    }

} // namespace

// ============================================================
// 时间源
// ============================================================

TEST(TimerNow, IsMonotonic) {
    const auto a = ITimer::now();
    const auto b = ITimer::now();
    EXPECT_LE(a, b);
}

// ============================================================
// 两者共有的语义
// ============================================================

template <typename T>
// ReSharper disable once CppUseInternalLinkage
class TimerTest : public ::testing::Test {};

using TimerImpls = ::testing::Types<SleepTimer, SpinTimer>;
TYPED_TEST_SUITE(TimerTest, TimerImpls);

// d <= 0 立即返回 true（没东西可等）
TYPED_TEST(TimerTest, NonPositiveDurationReturnsTrueImmediately) {
    TypeParam timer;
    std::stop_source src;

    EXPECT_TRUE(timer.wait(Duration(0), src.get_token()));
    EXPECT_TRUE(timer.wait(Duration(-5), src.get_token()));
}

// 进入等待前就被叫停 → 立即 false，不睡
TYPED_TEST(TimerTest, AlreadyStoppedReturnsFalseImmediately) {
    TypeParam timer;
    std::stop_source src;
    src.request_stop();

    const auto elapsed = measureMs([&] {
        EXPECT_FALSE(timer.wait(Duration(500), src.get_token()));
    });
    EXPECT_LT(elapsed, 20) << "已停止却还在等";
}

// 正常等待要等满（下限严格，上界放宽）
TYPED_TEST(TimerTest, WaitsFullDuration) {
    TypeParam timer;
    std::stop_source src;

    const auto elapsed = measureMs([&] {
        EXPECT_TRUE(timer.wait(Duration(50), src.get_token()));
    });

    EXPECT_GE(elapsed, 50) << "提前返回了";
    EXPECT_LT(elapsed, 250) << "睡过头太多";
}

// 无状态：一次 stop 不影响后续用新 stop_source 的调用
TYPED_TEST(TimerTest, IndependentOfPreviousStopState) {
    TypeParam timer;

    std::stop_source stopped;
    stopped.request_stop();
    EXPECT_FALSE(timer.wait(Duration(10), stopped.get_token()));

    std::stop_source alive;
    EXPECT_TRUE(timer.wait(Duration(10), alive.get_token()));
}

// 无状态：同一实例上两个线程并发等待都应正常完成，且真的并行
TYPED_TEST(TimerTest, ConcurrentWaitsOnSameInstance) {
    TypeParam timer;
    std::stop_source src;

    std::atomic<int> completed{0};
    const auto elapsed = measureMs([&] {
        std::jthread a([&] {
            if (timer.wait(Duration(100), src.get_token())) ++completed;
        });
        std::jthread b([&] {
            if (timer.wait(Duration(100), src.get_token())) ++completed;
        });
        a.join();
        b.join();
    });

    EXPECT_EQ(completed.load(), 2);
    EXPECT_LT(elapsed, 180) << "两个 wait 看起来是串行执行的";
}

// ============================================================
// SleepTimer 特有：停止响应延迟上界 = 一个 slice 对齐到系统节拍 + 裕量
// ============================================================

TEST(SleepTimerTest, StopDuringWaitReturnsFalseQuickly) {
    SleepTimer timer;

    const auto latency = stopLatencyMs(timer, Duration(5000));
    ASSERT_GE(latency, 0) << "没被打断，等满了";

    // slice 10ms + 系统节拍粒度 + 调度抖动
    EXPECT_LT(latency, SleepTimer::kSlice.count() + 50);

}

// ============================================================
// SpinTimer 特有：精度与极短的停止响应
// ============================================================

TEST(SpinTimerTest, ShortWaitIsAccurate) {
    SpinTimer timer;
    std::stop_source src;

    const auto elapsed = measureMs([&] {
        EXPECT_TRUE(timer.wait(Duration(5), src.get_token()));
    });

    EXPECT_GE(elapsed, 5) << "提前返回了";
    EXPECT_LT(elapsed, 20) << "忙等不该睡过头";
}

TEST(SpinTimerTest, StopDuringWaitReturnsFalseQuickly) {
    SpinTimer timer;

    const auto latency = stopLatencyMs(timer, Duration(5000));
    ASSERT_GE(latency, 0) << "没被打断，等满了";

    // 亚微秒级响应，留 20ms 给线程调度
    EXPECT_LT(latency, 20);
}

// ============================================================
// HybridTimer 特有：语义依赖构造时给的 spinTail
// ============================================================

TEST(HybridTimerTest, NonPositiveDurationReturnsTrueImmediately) {
    const HybridTimer timer(Duration(10));
    std::stop_source src;

    EXPECT_TRUE(timer.wait(Duration(0), src.get_token()));
    EXPECT_TRUE(timer.wait(Duration(-5), src.get_token()));
}

TEST(HybridTimerTest, AlreadyStoppedReturnsFalseImmediately) {
    const HybridTimer timer(Duration(10));
    std::stop_source src;
    src.request_stop();

    const auto elapsed = measureMs([&] {
        EXPECT_FALSE(timer.wait(Duration(500), src.get_token()));
    });
    EXPECT_LT(elapsed, 20) << "已停止却还在等";
}

// 等待时长落在尾巴里 → 全程忙等，精度应与 SpinTimer 一致
TEST(HybridTimerTest, ShortWaitWithinTailIsPureSpin) {
    const HybridTimer timer(Duration(10));
    std::stop_source src;

    const auto elapsed = measureMs([&] {
        EXPECT_TRUE(timer.wait(Duration(5), src.get_token()));
    });

    EXPECT_GE(elapsed, 5) << "提前返回了";
    EXPECT_LT(elapsed, 20) << "这段本该是忙等";
}

// 长于尾巴 → 睡眠 + 忙等；尾巴要能把睡眠溢出兜住
TEST(HybridTimerTest, LongWaitEndsCloseToDeadline) {
    const HybridTimer timer(Duration(10));
    std::stop_source src;

    const auto elapsed = measureMs([&] {
        EXPECT_TRUE(timer.wait(Duration(60), src.get_token()));
    });

    EXPECT_GE(elapsed, 60) << "提前返回了";
    EXPECT_LT(elapsed, 60 + 10 + 50) << "尾巴没兜住睡眠溢出";
}

// 停止响应上界 ≈ spinTail + 一个 slice
TEST(HybridTimerTest, StopDuringWaitReturnsFalseQuickly) {
    HybridTimer timer(Duration(10));

    const auto latency = stopLatencyMs(timer, Duration(5000));
    ASSERT_GE(latency, 0) << "没被打断，等满了";

    EXPECT_LT(latency, Duration(10).count() + SleepTimer::kSlice.count() + 50);
}

// spinTail <= 0 → 退化为纯分段睡眠
TEST(HybridTimerTest, ZeroTailBehavesLikeSleepTimer) {
    const HybridTimer timer(Duration(0));
    std::stop_source src;

    const auto elapsed = measureMs([&] {
        EXPECT_TRUE(timer.wait(Duration(50), src.get_token()));
    });

    EXPECT_GE(elapsed, 50) << "提前返回了";
    EXPECT_LT(elapsed, 250) << "睡过头太多";
}
