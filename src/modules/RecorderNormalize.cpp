#include "modules/RecorderNormalize.hpp"

#include <cstdlib>
#include <unordered_set>
#include <utility>

#include "common/Hotkey.hpp"

namespace {

    // 位移超过这么多像素就算拖动，否则压成一次点击
    constexpr int kDragThreshold = 2;

    // 滚轮一格的粒度
    constexpr int kWheelDelta = 120;

    // 这条事件是不是"某个热键的修饰键"
    [[nodiscard]] bool isModifierEventOf(const recorder::RawEvent &e, const unsigned mask) noexcept {
        if (mask == 0) return false;
        if (e.kind != recorder::RawKind::KeyDown && e.kind != recorder::RawKind::KeyUp) return false;

        const unsigned bit = utils::modifierBitForVk(e.vk);
        return bit != 0 && (bit & mask) != 0;
    }

} // namespace

namespace recorder {

std::vector<Action> normalize(const std::vector<RawEvent> &events, const NormalizeOptions &opts) {
    std::vector<Action> out;
    if (events.empty()) return out;

    // ---- 1. 头尾剔除 ----
    // 头部是"开始热键的修饰键"（用户在按热键之前就按下了 Ctrl 之类），
    // 尾部是"停止热键的修饰键"（停止热键本身已经把线程停了，松开动作是它的回声）。
    std::size_t begin = 0;
    std::size_t end = events.size();

    while (begin < end && isModifierEventOf(events[begin], opts.startMods)) ++begin;
    while (end > begin && isModifierEventOf(events[end - 1], opts.stopMods)) --end;

    // ---- 2. 逐事件归一化 ----
    bool haveLast = false;
    std::uint32_t lastTime = 0;

    const auto push = [&](Action action, const std::uint32_t time) {
        action.delayMs = haveLast ? static_cast<int>(time - lastTime) : 0;
        haveLast = true;
        lastTime = time;
        out.push_back(std::move(action)); // NOLINT(*-move-const-arg)
    };

    const auto limitReached = [&] {
        return opts.maxActions > 0 && static_cast<int>(out.size()) >= opts.maxActions;
    };

    // 键盘：还按着的键（自动重复合并 + 丢弃没有对应 Down 的 Up）
    std::unordered_set<unsigned> keysDown;

    // 鼠标：挂起中的按下（可能压成点击，也可能升级成拖动）
    bool havePendingDown = false;
    MouseButton pendingButton = MouseButton::Left;
    int pendingX = 0;
    int pendingY = 0;
    std::uint32_t pendingTime = 0;
    bool dragging = false;

    // 鼠标：已经落地成 MouseDown 的键（多键同按时用），保证 Up 能配对
    std::unordered_set<MouseButton> emittedButtons;

    // 滚轮：不足一格的残量
    int wheelPending = 0;
    bool wheelHorizontal = false;

    for (std::size_t i = begin; i < end && !limitReached(); ++i) {
        const RawEvent &e = events[i];

        // ---- 键盘 ----
        if (e.kind == RawKind::KeyDown || e.kind == RawKind::KeyUp) {
            // 开始/停止热键的主键永不进产物（钩子已经滤过一遍，这里是第二道，
            // 同时让本函数自身完备、可单测）
            if ((opts.startKeyVk != 0 && e.vk == opts.startKeyVk) ||
                (opts.stopKeyVk != 0 && e.vk == opts.stopKeyVk)) {
                continue;
            }

            if (e.kind == RawKind::KeyDown) {
                // 自动重复：按住不放会连发 Down，只留第一次
                if (!keysDown.insert(e.vk).second) continue;
            } else {
                // 没有对应 Down 的 Up（比如录制开始前就按着）直接丢，
                // 否则回放会去松一个从没按过的键
                if (keysDown.erase(e.vk) == 0) continue;
            }

            push(Action{.type = ActionType::Keyboard,
                        .hotkey = Hotkey{.modifiers = 0, .vk = e.vk},
                        .keyEvent = (e.kind == RawKind::KeyDown) ? KeyEventType::Down
                                                                 : KeyEventType::Up},
                 e.time);
            continue;
        }

        // ---- 鼠标移动 ----
        if (e.kind == RawKind::Move) {
            if (havePendingDown && !dragging) {
                if (std::abs(e.x - pendingX) <= kDragThreshold &&
                    std::abs(e.y - pendingY) <= kDragThreshold) {
                    continue;   // 抖动，不算拖动
                }

                // 越过阈值：先补按下（起点），再补这一条移动
                push(Action{.type = ActionType::MouseDown,
                            .x = pendingX,
                            .y = pendingY,
                            .button = pendingButton},
                     pendingTime);
                emittedButtons.insert(pendingButton);
                dragging = true;
            }

            push(Action{.type = ActionType::MouseMove, .x = e.x, .y = e.y}, e.time);
            continue;
        }

        // ---- 鼠标按下 ----
        if (e.kind == RawKind::ButtonDown) {
            if (havePendingDown) {
                // 已经按着一个键了：这个键直接落地，不参与点击合并（多键同按）
                push(Action{.type = ActionType::MouseDown,
                            .x = e.x,
                            .y = e.y,
                            .button = e.button},
                     e.time);
                emittedButtons.insert(e.button);
                continue;
            }

            havePendingDown = true;
            dragging = false;
            pendingButton = e.button;
            pendingX = e.x;
            pendingY = e.y;
            pendingTime = e.time;
            continue;
        }

        // ---- 鼠标松开 ----
        if (e.kind == RawKind::ButtonUp) {
            if (havePendingDown && e.button == pendingButton) {
                const bool moved = dragging || std::abs(e.x - pendingX) > kDragThreshold ||
                                   std::abs(e.y - pendingY) > kDragThreshold;

                if (moved) {
                    if (!dragging) {
                        // 一步就拖到位了，中间没有 Move 事件：把起点补上
                        push(Action{.type = ActionType::MouseDown,
                                    .x = pendingX,
                                    .y = pendingY,
                                    .button = pendingButton},
                             pendingTime);
                    }
                    push(Action{.type = ActionType::MouseUp,
                                .x = e.x,
                                .y = e.y,
                                .button = e.button},
                         e.time);
                } else {
                    // 原地按松：压成一条点击，按住时长取真实间隔
                    auto click = Action{.type = ActionType::MouseClick,
                                        .x = pendingX,
                                        .y = pendingY,
                                        .button = pendingButton};
                    click.holdMs = static_cast<int>(e.time - pendingTime);
                    push(click, e.time);
                }

                havePendingDown = false;
                dragging = false;
                continue;
            }

            // 不是挂起中的那个键：只在这个键真的落下过 MouseDown 时才补 Up
            if (emittedButtons.erase(e.button) > 0) {
                push(Action{.type = ActionType::MouseUp, .x = e.x, .y = e.y, .button = e.button},
                     e.time);
            }
            continue;
        }

        // ---- 滚轮 ----
        if (e.kind == RawKind::Wheel) {
            if (wheelHorizontal != e.horizontal && wheelPending != 0) {
                // 横竖方向切换时，不足一格的残量丢弃（极少见，不替用户凑整）
                wheelPending = 0;
            }
            wheelHorizontal = e.horizontal;
            wheelPending += e.wheelDelta;

            while (std::abs(wheelPending) >= kWheelDelta) {
                const int notches = wheelPending / kWheelDelta;
                wheelPending -= notches * kWheelDelta;

                const bool positive = notches > 0;
                const WheelDirection dir =
                    wheelHorizontal
                        ? (positive ? WheelDirection::Right : WheelDirection::Left)
                        : (positive ? WheelDirection::Up : WheelDirection::Down);

                push(Action{.type = ActionType::MouseWheel,
                            .wheelDirection = dir,
                            .wheelSteps = std::abs(notches)},
                     e.time);

                if (limitReached()) return out;
            }
        }
    }

    // 收尾时还按着的键、还挂起的按下、不足一格的滚轮都不产出：
    // 录制端不替用户补动作，回放端的补平机制负责收尾。
    return out;
}

} // namespace recorder
