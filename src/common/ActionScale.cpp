#include "common/ActionScale.hpp"

#include <algorithm>
#include <cmath>

namespace {

    // 单个时间字段缩放：保留符号语义（<= 0 一律回到 0），四舍五入到毫秒。
    [[nodiscard]] int scaleMs(const int ms, const double factor) {
        if (ms <= 0) {
            return 0;
        }

        const double scaled = static_cast<double>(ms) / factor;
        return std::max(0, static_cast<int>(std::lround(scaled)));
    }

} // namespace

void action::scaleActions(std::vector<Action> &actions, const double factor) {
    if (!(factor > 0.0) || factor == 1.0) {
        return;
    }

    for (Action &a: actions) {
        a.delayMs = scaleMs(a.delayMs, factor);

        // 只有鼠标点击带"按住时长"；键盘是 down/up 两条独立动作，没有 holdMs。
        if (a.type == ActionType::MouseClick && a.holdMs > 0) {
            a.holdMs = std::max(kMinHoldMs, scaleMs(a.holdMs, factor));
        }
    }
}
