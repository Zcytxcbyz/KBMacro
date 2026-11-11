#pragma once

#include <vector>

#include "common/Action.hpp"

// 按倍率缩放动作序列里的时间字段（回放倍率用）。
//
//   factor > 1  播放加快，时间变短
//   factor < 1  播放放慢，时间变长
//   factor == 1 原样不动（不会产生任何数值变化）
//
// 只动 delayMs 与 holdMs，坐标、按键、滚轮步数一律不变。
// 缩放后 holdMs 夹到 >= kMinHoldMs：太快时 down/up 会被系统合并成一次点击。
// 缩放后 delayMs 夹到 >= 0。
namespace action {

    // 按住时长的下限（毫秒）。低于这个值，down 与 up 之间可能没有可观测的间隔。
    inline constexpr int kMinHoldMs = 5;

    // 就地缩放。factor <= 0 或为 1 时什么都不做。
    void scaleActions(std::vector<Action> &actions, double factor);

} // namespace action
