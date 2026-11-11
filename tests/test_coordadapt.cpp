// ============================================================
// Coordinate adaptation tests: pure functions, values only.
//   - only used when importing someone else's script, so inputs are always "two screen specs"
//   - MulDiv rounds half away from zero
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
    return Action{
            .type = ActionType::MouseClick, .x = x, .y = y, .button = MouseButton::Left, .holdMs = 50, .delayMs = 25};
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

    EXPECT_EQ(actions[0].x, 51); // 50.5 rounded
    EXPECT_EQ(actions[0].y, 26); // 25.5 rounded
}

TEST(CoordAdapt, RoundsHalfAwayFromZero) {
    // 200 → 100: half of the positions land on .5
    std::vector<Action> actions{move(1, 3), move(5, 0)};

    coord::adaptActions(actions, spec(200, 200), spec(100, 100));

    EXPECT_EQ(actions[0].x, 1); // 0.5 → 1
    EXPECT_EQ(actions[0].y, 2); // 1.5 → 2
    EXPECT_EQ(actions[1].x, 3); // 2.5 → 3
    EXPECT_EQ(actions[1].y, 0);
}

TEST(CoordAdapt, NonUniformScales) {
    std::vector<Action> actions{move(100, 100)};

    coord::adaptActions(actions, spec(1000, 1000), spec(2000, 500));

    EXPECT_EQ(actions[0].x, 200);
    EXPECT_EQ(actions[0].y, 50);
}

TEST(CoordAdapt, IgnoresNonCoordinateActions) {
    // wheel and keyboard x/y must not be scaled even if hand-edited to non-zero
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
    // second screen left of the main one: coordinates are already negative, scale as-is, no clamp
    std::vector<Action> actions{move(-100, -50)};

    coord::adaptActions(actions, spec(1920, 1080), spec(3840, 2160));

    EXPECT_EQ(actions[0].x, -200);
    EXPECT_EQ(actions[0].y, -100);
}

TEST(CoordAdapt, AdaptsDragEndpoints) {
    std::vector<Action> actions{
            Action{.type = ActionType::MouseDown, .x = 100, .y = 50, .button = MouseButton::Left},
            Action{.type = ActionType::MouseUp, .x = 300, .y = 150, .button = MouseButton::Left},
    };

    coord::adaptActions(actions,
                        ScreenSpec{.width = 1000, .height = 1000, .dpi = 96},
                        ScreenSpec{.width = 2000, .height = 2000, .dpi = 96});

    EXPECT_EQ(actions[0].x, 200);
    EXPECT_EQ(actions[0].y, 100);
    EXPECT_EQ(actions[1].x, 600);
    EXPECT_EQ(actions[1].y, 300);
}

TEST(CoordAdapt, SkipsRelativeActions) {
    std::vector<Action> actions{
            Action{.type = ActionType::MouseMove, .x = 640, .y = 360},
            Action{.type = ActionType::MouseMove, .x = 640, .y = 360, .relative = true},
            Action{.type = ActionType::MouseClick,
                   .x = -20,
                   .y = 10,
                   .relative = true,
                   .button = MouseButton::Left,
                   .holdMs = 50},
    };

    coord::adaptActions(actions, spec(1280, 720), spec(2560, 1440));

    // absolute actions are doubled
    EXPECT_EQ(actions[0].x, 1280);
    EXPECT_EQ(actions[0].y, 720);
    // relative actions are offsets, kept as-is
    EXPECT_EQ(actions[1].x, 640);
    EXPECT_EQ(actions[1].y, 360);
    EXPECT_EQ(actions[2].x, -20);
    EXPECT_EQ(actions[2].y, 10);
}
