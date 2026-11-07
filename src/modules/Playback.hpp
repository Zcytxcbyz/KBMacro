#pragma once

#include <stop_token>
#include <vector>

#include "common/Action.hpp"
#include "common/ITimer.hpp"
#include "common/Types.hpp"

// 回放：把一串动作按各自的时间间隔重新发出去。
//
// 契约：
//   - 只读 actions，不持有、不修改
//   - 不认识 ThreadManager / 面板
//   - 循环体不打日志（input 层已经逐条记录）
//   - input::SendInputError 往外抛，由 ThreadManager 兜成 FinishReason::Failed
//   - 返回实际播放的动作条数（不是轮数）
//
// ActionType 说明：
//   - delayMs 是"上一条动作完全做完之后再等这么久"，所以回放里它是纯前置等待
//   - MouseClick 会先移动到 x/y 再点
//   - 将来的相对坐标是新的 ActionType 值，本文件对未知类型按无操作处理
namespace playback {

    [[nodiscard]] int run(const std::vector<Action> &actions, const RunSpec &run,
                          const ITimer &timer, const std::stop_token &st);

} // namespace playback
