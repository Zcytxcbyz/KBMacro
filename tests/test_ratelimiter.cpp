#include <gtest/gtest.h>

#include "common/RateLimiter.hpp"

namespace {

    using Duration = ITimer::Duration;
    using TimePoint = ITimer::TimePoint;

} // namespace

TEST(RateLimiterTest, FirstCallIsAllowed) {
    RateLimiter limiter{Duration{1000}};

    EXPECT_TRUE(limiter.allow(TimePoint{}));
}

TEST(RateLimiterTest, RejectsBeforeInterval) {
    RateLimiter limiter{Duration{1000}};
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const TimePoint base{};

    ASSERT_TRUE(limiter.allow(base));
    EXPECT_FALSE(limiter.allow(base + Duration{999}));
}

TEST(RateLimiterTest, AllowsExactlyAtInterval) {
    RateLimiter limiter{Duration{1000}};
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const TimePoint base{};

    ASSERT_TRUE(limiter.allow(base));
    // 边界是"大于等于"，所以正好 1000ms 要放行
    EXPECT_TRUE(limiter.allow(base + Duration{1000}));
}

TEST(RateLimiterTest, AllowsAgainAfterInterval) {
    RateLimiter limiter{Duration{1000}};
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const TimePoint base{};

    ASSERT_TRUE(limiter.allow(base));
    ASSERT_TRUE(limiter.allow(base + Duration{1500}));  // 放行，起点移到 1500
    EXPECT_FALSE(limiter.allow(base + Duration{2000})); // 只过了 500ms
    EXPECT_TRUE(limiter.allow(base + Duration{2500}));  // 又过了 1000ms
}

TEST(RateLimiterTest, ResetAllowsImmediately) {
    RateLimiter limiter{Duration{1000}};
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const TimePoint base{};

    ASSERT_TRUE(limiter.allow(base));
    ASSERT_FALSE(limiter.allow(base + Duration{10}));

    limiter.reset();
    EXPECT_TRUE(limiter.allow(base + Duration{20}));
}

TEST(RateLimiterTest, ZeroIntervalAlwaysAllows) {
    RateLimiter limiter{Duration{0}};
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const TimePoint base{};

    EXPECT_TRUE(limiter.allow(base));
    EXPECT_TRUE(limiter.allow(base));
    EXPECT_TRUE(limiter.allow(base + Duration{1}));
}

TEST(RateLimiterTest, IntervalIsKept) {
    const RateLimiter limiter{Duration{250}};

    EXPECT_EQ(limiter.interval().count(), 250);
}
