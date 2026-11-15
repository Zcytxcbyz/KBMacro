#include "common/ActionScale.hpp"

#include <algorithm>
// ReSharper disable once CppUnusedIncludeDirective
#include <cmath>

namespace {

// scale one time field: preserve sign semantics (<= 0 always maps to 0), round to whole ms.
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

    for (Action &a : actions) {
        a.delayMs = scaleMs(a.delayMs, factor);

        // only mouse clicks carry a "hold duration"; keyboard is separate down/up actions without holdMs.
        if (a.type == ActionType::MouseClick && a.holdMs > 0) {
            a.holdMs = std::max(kMinHoldMs, scaleMs(a.holdMs, factor));
        }
    }
}
