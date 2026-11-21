// ============================================================
// ITimer implementation tests (SleepTimer / SpinTimer / HybridTimer)
//
// rules for writing timing assertions:
//   - a lower-bound assertion ("not earlier than X") is meaningful and must be strict: no early return
//   - upper-bound assertions need slack: Windows timer granularity is ~15.6ms, sleep_for oversleeps;
//     machine load jitter also stretches the measured value; a tight upper bound only yields flaky cases
//
// shared semantics run twice via TYPED_TEST; implementation-specific ones (latency bound, precision) are separate.
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

// measure the elapsed time of a piece of code (milliseconds)
template<typename Fn> long long measureMs(Fn &&fn) {
    const auto t0 = Clock::now();
    fn();
    return std::chrono::duration_cast<Duration>(Clock::now() - t0).count();
}

// put the timer into a wait, request stop after 30ms, return the measured stop->wait latency.
// returning -1 means wait was not interrupted by stop (waited fully); the test fails on that.
// ReSharper disable once CppParameterMayBeConstPtrOrRef
long long stopLatencyMs(ITimer &timer, const Duration d) {
    std::stop_source src;
    bool interrupted = false;

    std::jthread worker([&] {
        interrupted = !timer.wait(d, src.get_token()); // wait returned false = interrupted
    });

    // let the worker actually enter the wait first, then request stop
    std::this_thread::sleep_for(Duration(30));

    const auto t0 = Clock::now();
    src.request_stop();
    worker.join(); // join returns ~ wait returns

    if (!interrupted) return -1;
    return std::chrono::duration_cast<Duration>(Clock::now() - t0).count();
}

} // namespace

// ============================================================
// Time source
// ============================================================

TEST(TimerNow, IsMonotonic) {
    const auto a = ITimer::now();
    const auto b = ITimer::now();
    EXPECT_LE(a, b);
}

// ============================================================
// Semantics shared by both
// ============================================================

template<typename T>
// ReSharper disable once CppUseInternalLinkage
class TimerTest : public ::testing::Test {};

using TimerImpls = ::testing::Types<SleepTimer, SpinTimer>;
TYPED_TEST_SUITE(TimerTest, TimerImpls);

// d <= 0 returns true immediately (nothing to wait for)
TYPED_TEST(TimerTest, NonPositiveDurationReturnsTrueImmediately) {
    TypeParam timer;
    std::stop_source src;

    EXPECT_TRUE(timer.wait(Duration(0), src.get_token()));
    EXPECT_TRUE(timer.wait(Duration(-5), src.get_token()));
}

// stopped before entering the wait -> false immediately, no sleep
TYPED_TEST(TimerTest, AlreadyStoppedReturnsFalseImmediately) {
    TypeParam timer;
    std::stop_source src;
    src.request_stop();

    const auto elapsed = measureMs([&] { EXPECT_FALSE(timer.wait(Duration(500), src.get_token())); });
    EXPECT_LT(elapsed, 20) << "stop was requested but the wait kept going";
}

// a normal wait must last the full duration (strict lower bound, loose upper bound)
TYPED_TEST(TimerTest, WaitsFullDuration) {
    TypeParam timer;
    std::stop_source src;

    const auto elapsed = measureMs([&] { EXPECT_TRUE(timer.wait(Duration(50), src.get_token())); });

    EXPECT_GE(elapsed, 50) << "returned before the deadline";
    EXPECT_LT(elapsed, 250) << "overslept too much";
}

// stateless: one stop does not affect later calls with a new stop_source
TYPED_TEST(TimerTest, IndependentOfPreviousStopState) {
    TypeParam timer;

    std::stop_source stopped;
    stopped.request_stop();
    EXPECT_FALSE(timer.wait(Duration(10), stopped.get_token()));

    std::stop_source alive;
    EXPECT_TRUE(timer.wait(Duration(10), alive.get_token()));
}

// stateless: two threads waiting concurrently on one instance both finish, and truly in parallel
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
    EXPECT_LT(elapsed, 180) << "the two waits look serialized";
}

// ============================================================
// SleepTimer specific: stop-response latency bound = one slice aligned to the system tick + slack
// ============================================================

TEST(SleepTimerTest, StopDuringWaitReturnsFalseQuickly) {
    SleepTimer timer;

    const auto latency = stopLatencyMs(timer, Duration(5000));
    ASSERT_GE(latency, 0) << "not interrupted, waited the full duration";

    // slice 10ms + system tick granularity + scheduling jitter
    EXPECT_LT(latency, SleepTimer::kSlice.count() + 50);
}

// ============================================================
// SpinTimer specific: precision and a very short stop response
// ============================================================

TEST(SpinTimerTest, ShortWaitIsAccurate) {
    SpinTimer timer;
    std::stop_source src;

    const auto elapsed = measureMs([&] { EXPECT_TRUE(timer.wait(Duration(5), src.get_token())); });

    EXPECT_GE(elapsed, 5) << "returned before the deadline";
    EXPECT_LT(elapsed, 20) << "a busy wait must not oversleep";
}

TEST(SpinTimerTest, StopDuringWaitReturnsFalseQuickly) {
    SpinTimer timer;

    const auto latency = stopLatencyMs(timer, Duration(5000));
    ASSERT_GE(latency, 0) << "not interrupted, waited the full duration";

    // sub-microsecond response; 20ms is left for thread scheduling
    EXPECT_LT(latency, 20);
}

// ============================================================
// HybridTimer specific: semantics depend on the spinTail given at construction
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

    const auto elapsed = measureMs([&] { EXPECT_FALSE(timer.wait(Duration(500), src.get_token())); });
    EXPECT_LT(elapsed, 20) << "stop was requested but the wait kept going";
}

// wait falls inside the tail -> full spin; precision should match SpinTimer
TEST(HybridTimerTest, ShortWaitWithinTailIsPureSpin) {
    const HybridTimer timer(Duration(10));
    std::stop_source src;

    const auto elapsed = measureMs([&] { EXPECT_TRUE(timer.wait(Duration(5), src.get_token())); });

    EXPECT_GE(elapsed, 5) << "returned before the deadline";
    EXPECT_LT(elapsed, 20) << "this part should be a busy wait";
}

// longer than the tail -> sleep + spin; the tail must absorb the sleep overshoot
TEST(HybridTimerTest, LongWaitEndsCloseToDeadline) {
    const HybridTimer timer(Duration(10));
    std::stop_source src;

    const auto elapsed = measureMs([&] { EXPECT_TRUE(timer.wait(Duration(60), src.get_token())); });

    EXPECT_GE(elapsed, 60) << "returned before the deadline";
    EXPECT_LT(elapsed, 60 + 10 + 50) << "the spin tail did not absorb the sleep overshoot";
}

// stop-response bound ~ spinTail + one slice
TEST(HybridTimerTest, StopDuringWaitReturnsFalseQuickly) {
    HybridTimer timer(Duration(10));

    const auto latency = stopLatencyMs(timer, Duration(5000));
    ASSERT_GE(latency, 0) << "not interrupted, waited the full duration";

    EXPECT_LT(latency, Duration(10).count() + SleepTimer::kSlice.count() + 50);
}

// spinTail <= 0 -> degrades to pure sliced sleep
TEST(HybridTimerTest, ZeroTailBehavesLikeSleepTimer) {
    const HybridTimer timer(Duration(0));
    std::stop_source src;

    const auto elapsed = measureMs([&] { EXPECT_TRUE(timer.wait(Duration(50), src.get_token())); });

    EXPECT_GE(elapsed, 50) << "returned before the deadline";
    EXPECT_LT(elapsed, 250) << "overslept too much";
}
