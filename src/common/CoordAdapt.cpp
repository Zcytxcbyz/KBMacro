#include "common/CoordAdapt.hpp"

#include <windows.h> // MulDiv

#include "common/Log.hpp"

namespace coord {

void adaptActions(std::vector<Action> &actions, const ScreenSpec &from, const ScreenSpec &to) {
    if (from.width <= 0 || from.height <= 0) {
        LOG_WARN("coord: source screen spec invalid ({}x{}), coordinates left untouched", from.width, from.height);
        return;
    }

    int adapted = 0;
    for (Action &action : actions) {
        // relative coordinates are offsets and independent of resolution; do not scale them
        if (action.relative) continue;

        // both ends of a drag carry coordinates and are scaled together
        const bool hasCoords = (action.type == ActionType::MouseMove || action.type == ActionType::MouseClick ||
                                action.type == ActionType::MouseDown || action.type == ActionType::MouseUp);
        if (!hasCoords) continue;

        action.x = ::MulDiv(action.x, to.width, from.width);
        action.y = ::MulDiv(action.y, to.height, from.height);
        ++adapted;
    }

    LOG_DEBUG("coord: adapted {} of {} actions from {}x{} to {}x{}",
              adapted,
              actions.size(),
              from.width,
              from.height,
              to.width,
              to.height);
}

} // namespace coord
