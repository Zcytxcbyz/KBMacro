// ============================================================
// 键盘模块测试：
//   - 轮数口径、轮间等待、被 stop 打断
//   - 空热键不拒绝（模块自己的决策：合法性归面板）
//
// 同连点器：input::pressKey 内部的按住等待也走这个 FakeTimer，
// 所以按住的 5ms 与轮间隔的 95ms 要分开数。
// 不测 Timed 的轮数（ITimer::now() 是静态的，改不了时钟）。
// ============================================================

#include <gtest/gtest.h>

#include "common/KeyboardParams.hpp"

#include "modules/Keyboard.hpp"

#include "FakeTimer.hpp"

namespace {

    // intervalMs = 100、holdMs = 5 → 轮间隔 95，与按住的 5ms 不同
    KeyboardParams makeParams(const RunMode mode, const int count,
                              const Hotkey &hotkey = Hotkey{.vk = 0x41}) {   // 默认 A
        KeyboardParams p;
        p.hotkey = hotkey;
        p.intervalMs = 100;
        p.holdMs = 5;
        p.run = RunSpec{.mode = mode, .count = count};
        return p;
    }

    constexpr ITimer::Duration kHold{5};    // 一次按住的等待
    constexpr ITimer::Duration kGap{95};    // 轮间隔（100 - 5）

} // namespace

TEST(KeyboardRun, CountedRunsExactRounds) {
    FakeTimer timer;
    const KeyboardParams p = makeParams(RunMode::Counted, 3);

    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 3);

    // 三次按住等待 + 两次轮间隔（最后一轮之后不再等）
    EXPECT_EQ(timer.countWaits(kHold), 3u);
    EXPECT_EQ(timer.countWaits(kGap), 2u);
    EXPECT_EQ(timer.waitCalls(), 5);
}

TEST(KeyboardRun, CountedZeroDoesNothing) {
    FakeTimer timer;
    const KeyboardParams p = makeParams(RunMode::Counted, 0);

    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 0);
    EXPECT_EQ(timer.waitCalls(), 0);
}

TEST(KeyboardRun, GapDeductsHoldTime) {
    FakeTimer timer;
    const KeyboardParams p = makeParams(RunMode::Counted, 2);

    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 2);

    EXPECT_EQ(timer.countWaits(kGap), 1u);
    EXPECT_EQ(timer.countWaits(ITimer::Duration{100}), 0u);
}

TEST(KeyboardRun, StopOnFirstWaitCountsFirstRound) {
    FakeTimer timer(1);   // 第一次等待（第 1 轮的按住）就"被打断"
    const KeyboardParams p = makeParams(RunMode::Infinite, 0);

    // 按键已经发出并抬起了，所以算一轮
    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 1);
    EXPECT_EQ(timer.waitCalls(), 1);
}

TEST(KeyboardRun, InfiniteStopsOnThirdRound) {
    // 调用序列：hold, gap, hold, gap, hold, gap, ...
    // 第 6 次等待 = 第 3 轮之后的间隔
    FakeTimer timer(6);
    const KeyboardParams p = makeParams(RunMode::Infinite, 0);

    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 3);
    EXPECT_EQ(timer.countWaits(kHold), 3u);
}

TEST(KeyboardRun, EmptyHotkeyIsNotRejected) {
    FakeTimer timer;
    const KeyboardParams p = makeParams(RunMode::Counted, 1, Hotkey{});   // vk=0, 无修饰键

    // 模块不校验参数：空热键照跑一轮，事件表为空、只在 input 层记一条 skipped
    EXPECT_EQ(keyboard::run(p, timer, std::stop_token{}), 1);
    EXPECT_EQ(timer.countWaits(kHold), 1u);
}
