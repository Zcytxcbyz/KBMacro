#pragma once

#include <vector>

#include "common/Action.hpp"
#include "common/Types.hpp"

// 坐标适配：把动作里的屏幕坐标从一份屏幕规格换算到另一份。
//
// 只在"导入别人的 *.macro.json"时使用——录制永远产出当前机器的绝对坐标。
// 纯数据变换：不碰线程、计时器、Win32 输入。
namespace coord {

    // 按比例换算 MouseMove / MouseClick 的 x、y：
    //   newX = MulDiv(oldX, to.width,  from.width)
    //   newY = MulDiv(oldY, to.height, from.height)
    //
    // MulDiv 是四舍五入（0.5 向远离零的方向），整数除法会把缩小的结果系统性偏小。
    //
    // from 的宽或高 <= 0 时不做任何改动（ScreenSpec 全 0 表示"未记录"）。
    // 负坐标照比例换算，不夹取——副屏在主屏左侧时坐标本来就是负的。
    // 其余字段（wheelSteps / hotkey / keyEvent / holdMs / delayMs）一律不动。
    void adaptActions(std::vector<Action> &actions, const ScreenSpec &from, const ScreenSpec &to);

} // namespace coord
