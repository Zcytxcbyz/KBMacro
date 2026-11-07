// ============================================================
// 坐标适配测试：纯函数，只看数值。
//   - 只在导入别人的脚本时使用，因此本文件的输入都是"两份屏幕规格"
//   - MulDiv 是四舍五入（0.5 向远离零的方向）
// ============================================================

#include <gtest/gtest.h>

#include <vector>

#include "common/Action.hpp"
#include "common/CoordAdapt.hpp"
#include "common/Types.hpp"

namespace {

    ScreenSpec spec(const int width, const int height, const int dpi = 96) {
        return ScreenSpec{.width = width, .height = height, .dpi = dpi};
    }

    Action move(const int x, const int y) {
        return Action{.type = ActionType::MouseMove, .x = x, .y = y};
    }

    Action click(const int x, const int y) {
        return Action{.type = ActionType::MouseClick,
                      .x = x,
                      .y = y,
                      .button = MouseButton::Left,
                      .holdMs = 50,
                      .delayMs = 25};
    }

} // namespace

TEST(CoordAdapt, DoublesCoordinates) {
    std::vector<Action> actions{move(100, 50), click(960, 540)};

    coord::adaptActions(actions, spec(1920, 1080), spec(3840, 2160));

    EXPECT_EQ(actions[0].x, 200);
    EXPECT_EQ(actions[0].y, 100);
    EXPECT_EQ(actions[1].x, 1920);
    EXPECT_EQ(actions[1].y, 1080);
}

TEST(CoordAdapt, HalvesWithRounding) {
    std::vector<Action> actions{move(101, 51)};

    coord::adaptActions(actions, spec(3840, 2160), spec(1920, 1080));

    EXPECT_EQ(actions[0].x, 51);   // 50.5 四舍五入
    EXPECT_EQ(actions[0].y, 26);   // 25.5 四舍五入
}

TEST(CoordAdapt, RoundsHalfAwayFromZero) {
    // 200 → 100：一半的位置都落在 .5 上
    std::vector<Action> actions{move(1, 3), move(5, 0)};

    coord::adaptActions(actions, spec(200, 200), spec(100, 100));

    EXPECT_EQ(actions[0].x, 1);   // 0.5 → 1
    EXPECT_EQ(actions[0].y, 2);   // 1.5 → 2
    EXPECT_EQ(actions[1].x, 3);   // 2.5 → 3
    EXPECT_EQ(actions[1].y, 0);
}

TEST(CoordAdapt, NonUniformScales) {
    std::vector<Action> actions{move(100, 100)};

    coord::adaptActions(actions, spec(1000, 1000), spec(2000, 500));

    EXPECT_EQ(actions[0].x, 200);
    EXPECT_EQ(actions[0].y, 50);
}

TEST(CoordAdapt, IgnoresNonCoordinateActions) {
    // 滚轮与键盘的 x/y 即使被手写成非 0，也不该被换算
    Action wheel{.type = ActionType::MouseWheel, .x = 500, .y = 500};
    Action key{.type = ActionType::Keyboard, .x = 700, .y = 300};
    std::vector<Action> actions{wheel, key};

    coord::adaptActions(actions, spec(1920, 1080), spec(3840, 2160));

    EXPECT_EQ(actions[0].x, 500);
    EXPECT_EQ(actions[0].y, 500);
    EXPECT_EQ(actions[1].x, 700);
    EXPECT_EQ(actions[1].y, 300);
}

TEST(CoordAdapt, KeepsTimingFields) {
    std::vector<Action> actions{click(960, 540)};

    coord::adaptActions(actions, spec(1920, 1080), spec(3840, 2160));

    EXPECT_EQ(actions[0].holdMs, 50);
    EXPECT_EQ(actions[0].delayMs, 25);
    EXPECT_EQ(actions[0].button, MouseButton::Left);
}

TEST(CoordAdapt, InvalidSourceLeavesUntouched) {
    std::vector<Action> actions{move(100, 50)};

    coord::adaptActions(actions, spec(0, 0), spec(3840, 2160));

    EXPECT_EQ(actions[0].x, 100);
    EXPECT_EQ(actions[0].y, 50);
}

TEST(CoordAdapt, NegativeCoordinatesScale) {
    // 副屏在主屏左侧：坐标本来就是负的，照比例走，不夹取
    std::vector<Action> actions{move(-100, -50)};

    coord::adaptActions(actions, spec(1920, 1080), spec(3840, 2160));

    EXPECT_EQ(actions[0].x, -200);
    EXPECT_EQ(actions[0].y, -100);
}
