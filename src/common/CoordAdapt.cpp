#include "common/CoordAdapt.hpp"

#include <windows.h>   // MulDiv

#include "common/Log.hpp"

namespace coord {

    void adaptActions(std::vector<Action> &actions, const ScreenSpec &from, const ScreenSpec &to) {
        if (from.width <= 0 || from.height <= 0) {
            LOG_WARN("coord: source screen spec invalid ({}x{}), coordinates left untouched",
                     from.width, from.height);
            return;
        }

        // 规格一致时直接跳过，省一次遍历
        if (from.width == to.width && from.height == to.height) return;

        for (Action &action : actions) {
            if (action.type != ActionType::MouseMove && action.type != ActionType::MouseClick) continue;

            action.x = ::MulDiv(action.x, to.width, from.width);
            action.y = ::MulDiv(action.y, to.height, from.height);
        }

        LOG_DEBUG("coord: adapted {} actions from {}x{} to {}x{}", actions.size(), from.width,
                  from.height, to.width, to.height);
    }

} // namespace coord
