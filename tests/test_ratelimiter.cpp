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
    // the boundary is "greater or equal", so exactly 1000ms must pass
    EXPECT_TRUE(limiter.allow(base + Duration{1000}));
}

TEST(RateLimiterTest, AllowsAgainAfterInterval) {
    RateLimiter limiter{Duration{1000}};
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const TimePoint base{};

    ASSERT_TRUE(limiter.allow(base));
    ASSERT_TRUE(limiter.allow(base + Duration{1500})); // allowed; the anchor moves to 1500
    EXPECT_FALSE(limiter.allow(base + Duration{2000})); // only 500ms have passed
    EXPECT_TRUE(limiter.allow(base + Duration{2500})); // another 1000ms have passed
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
