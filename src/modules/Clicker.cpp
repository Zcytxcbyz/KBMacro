#include "modules/Clicker.hpp"

#include <algorithm>   // std::max

#include "common/Log.hpp"
#include "common/RunController.hpp"

#include "win32/Input.hpp"

namespace clicker {

ClickStep toClickStep(const ClickType type) noexcept {
    switch (type) {
        case ClickType::Left:
            return {.button = MouseButton::Left};
        case ClickType::Right:
            return {.button = MouseButton::Right};
        case ClickType::Middle:
            return {.button = MouseButton::Middle};

        // 双击 = 两次完整点击（各等一次 holdMs），不是靠系统双击时限
        case ClickType::Double:
            return {.times = 2, .button = MouseButton::Left};

        case ClickType::Side1:
            return {.button = MouseButton::Side1};
        case ClickType::Side2:
            return {.button = MouseButton::Side2};

        case ClickType::WheelUp:
            return {.wheel = true, .direction = WheelDirection::Up};
        case ClickType::WheelDown:
            return {.wheel = true, .direction = WheelDirection::Down};
        case ClickType::WheelLeft:
            return {.wheel = true, .direction = WheelDirection::Left};
        case ClickType::WheelRight:
            return {.wheel = true, .direction = WheelDirection::Right};
    }

    return {};   // 不可达：枚举已全部分支，留着是为了让编译器闭嘴
}

int run(const ClickerParams &p, const ITimer &timer, const std::stop_token &st) {
    const auto [wheel, times, button, direction] =
        toClickStep(p.clickType);

    // 轮间等待：intervalMs 是整个周期，按住时间从里面扣；
    // 滚轮没有"按住"，等待就是完整间隔。
    const int gapMs = wheel ? p.intervalMs : std::max(0, p.intervalMs - p.holdMs);

    RunController rc(p.run, ITimer::now());
    int executed = 0;

    while (rc.shouldRun(ITimer::now())) {
        // 停止请求已到：这一轮一个事件都不发。
        // （input::click 自己也会查，但这里先挡一次，"轮数"才数得准）
        if (st.stop_requested()) break;

        // 每轮先定位；勾了"忽略坐标"就不动光标
        if (!p.ignorePosition) input::moveTo(p.x, p.y);

        // times 只有 1（单击类）或 2（双击）：双击 = 连做两次完整点击、各等一次 holdMs。
        // 次数由 toClickStep 给出，循环体里不出现 ClickType。
        bool interrupted = false;
        for (int i = 0; i < times; ++i) {
            if (wheel) {
                input::wheel(direction, p.wheelSteps);
            } else if (!input::click(button, p.holdMs, timer, st)) {
                interrupted = true;   // 按住期间被 stop：抬起已补发
                break;
            }
        }

        // 被打断的那一轮也计数：按下（或滚轮）已经发出去了
        ++executed;
        rc.noteIteration();

        if (interrupted) break;

        if (!timer.wait(rc.clampWait(ITimer::Duration{gapMs}, ITimer::now()), st)) break;
    }

    LOG_DEBUG("clicker: run finished, executed={}", executed);
    return executed;
}

} // namespace clicker
