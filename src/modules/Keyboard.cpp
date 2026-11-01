#include "modules/Keyboard.hpp"

#include <algorithm>   // std::max

#include "common/Log.hpp"
#include "common/RunController.hpp"

#include "win32/Input.hpp"

namespace keyboard {

    int run(const KeyboardParams &p, const ITimer &timer, const std::stop_token &st) {
        // 轮间等待：intervalMs 是整个周期，按住时间从里面扣
        const int gapMs = std::max(0, p.intervalMs - p.holdMs);

        RunController rc(p.run, ITimer::now());
        int executed = 0;

        while (rc.shouldRun(ITimer::now())) {
            if (st.stop_requested()) break;   // 停止请求已到：这一轮一个事件都不发

            // pressKey 返回 false = 按住期间被 stop 打断（抬起已补发）
            const bool interrupted = !input::pressKey(p.hotkey, p.holdMs, timer, st);

            ++executed;   // 被打断的那一轮也计数：按键已经发出去并抬起了
            rc.noteIteration();

            if (interrupted) break;
            if (!rc.shouldRun(ITimer::now())) break;   // 最后一轮之后不再白等一个间隔

            if (!timer.wait(rc.clampWait(ITimer::Duration{gapMs}, ITimer::now()), st)) break;
        }

        LOG_DEBUG("keyboard: run finished, executed={}", executed);
        return executed;
    }

} // namespace keyboard
