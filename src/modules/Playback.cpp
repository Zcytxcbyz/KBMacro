#include "modules/Playback.hpp"

#include <algorithm>
#include <unordered_map>

#include "common/Log.hpp"
#include "common/RunController.hpp"
#include "win32/Input.hpp"

namespace {

    // 按键配对表的一项：存下完整 Hotkey（补抬起时原样发回去）与按下次数。
    struct HeldKey {
        Hotkey hotkey;
        int count = 0;
    };

    // 配对表的键：vk 在低 16 位、修饰键在高位。
    // vk == 0（只按修饰键）时也不会和别的组合撞上。
    [[nodiscard]] unsigned heldKeyOf(const Hotkey &hk) noexcept {
        return (hk.vk & 0xFFFFu) | (hk.modifiers << 16);
    }

    // 把所有还按着的键放掉。
    // 正常情况下表是空的；真有事可做说明脚本本身不配平，或者停止/定时截断
    // 正好落在 down 与 up 之间 —— 不补就会卡键。
    void releaseHeldKeys(std::unordered_map<unsigned, HeldKey> &held) {
        int released = 0;
        for (auto &val: held | std::views::values) {
            auto &[hotkey, count] = val;
            if (count <= 0) continue;

            input::keyUp(hotkey);
            count = 0;
            ++released;
        }

        if (released > 0) LOG_WARN("playback: released {} stuck key(s)", released);
    }

    // 鼠标按键配对表：MouseDown 记账、MouseUp 销账。
    // 键是按键本身，值是按下次数（脚本重复 down 同一键也不会提前松开）。
    void pressHeldButton(std::unordered_map<MouseButton, int> &held, const MouseButton button) {
        ++held[button];
    }

    // 把所有还按着的鼠标键放掉（每轮结束、线程退出各一次）。
    void releaseHeldButtons(std::unordered_map<MouseButton, int> &held) {
        int released = 0;
        for (auto &[button, count]: held) {
            if (count <= 0) continue;

            input::buttonUp(button);
            count = 0;
            ++released;
        }

        if (released > 0) LOG_WARN("playback: released {} stuck mouse button(s)", released);
    }

    // 把光标挪到动作指定的位置：
    //   relative == true  → 相对当前位置偏移（moveBy，不参与坐标适配）
    //   relative == false → 虚拟桌面绝对坐标（moveTo）
    void positionCursor(const Action &action) {
        if (action.relative) {
            input::moveBy(action.x, action.y);
        } else {
            input::moveTo(action.x, action.y);
        }
    }

    // 执行一条动作。返回 false = 被 stop 打断（只有 click 的按住等待会）。
    bool applyAction(const Action &action, const ITimer &timer, const std::stop_token &st,
                     std::unordered_map<unsigned, HeldKey> &held,
                     std::unordered_map<MouseButton, int> &heldButtons) {
        switch (action.type) {
            case ActionType::MouseMove:
                positionCursor(action);
                return true;

            case ActionType::MouseClick:
                // 录制产物自带前置 MouseMove，但手写或编辑过的脚本可能只有一条 click，
                // 所以这里自己先就位（坐标本来就在 Action 里）。
                positionCursor(action);
                return input::click(action.button, action.holdMs, timer, st);

            case ActionType::MouseWheel:
                input::wheel(action.wheelDirection, action.wheelSteps);
                return true;

            case ActionType::MouseDown:
                // 拖动起点也要就位（与 MouseClick 同理）
                positionCursor(action);
                input::buttonDown(action.button);
                pressHeldButton(heldButtons, action.button);
                return true;

            case ActionType::MouseUp:
                // 先移动到松开点，否则终点会偏回按下时的位置
                positionCursor(action);
                input::buttonUp(action.button);
                if (const auto it = heldButtons.find(action.button);
                    it != heldButtons.end() && it->second > 0) {
                    --it->second;
                    }
                return true;

            case ActionType::Keyboard: {
                if (action.keyEvent == KeyEventType::Down) {
                    input::keyDown(action.hotkey);
                    auto &item = held[heldKeyOf(action.hotkey)];
                    item.hotkey = action.hotkey;
                    ++item.count;
                } else {
                    input::keyUp(action.hotkey);
                    if (const auto it = held.find(heldKeyOf(action.hotkey));
                        it != held.end() && it->second.count > 0) {
                        --it->second.count;
                    }
                }
                return true;
            }

            case ActionType::Delay:
                // 纯等待：时长就是 delayMs，而 delayMs 在循环里已经作为
                // "动作前等待"执行过了，所以这里无事可做。
                // 单独成一种类型的意义是它在动作列表里是一条可见的行 ——
                // 插停顿不用去改相邻动作的延时值。
                return true;
        }

        // 所有类型都已覆盖，这里只是兜底
        return true;
    }

} // namespace

namespace playback {

int run(const std::vector<Action> &actions, const RunSpec &run, const ITimer &timer,
        const std::stop_token &st) {
    if (actions.empty()) {
        LOG_WARN("playback: empty action list, nothing to play");
        return 0;
    }

    // 开始延迟：整个任务一次，且必须在 RunController 构造之前 ——
    // 否则定时模式的时长会把这段延迟一起算进去。
    if (const int startDelayMs = std::max(0, run.startDelayMs); startDelayMs > 0) {
        if (!timer.wait(ITimer::Duration{startDelayMs}, st)) {
            LOG_DEBUG("playback: start delay interrupted, executed=0");
            return 0;
        }
    }

    RunController rc(run, ITimer::now());

    std::unordered_map<unsigned, HeldKey> held;
    std::unordered_map<MouseButton, int> heldButtons;
    int executed = 0;
    bool interrupted = false;

    while (!interrupted && rc.shouldRun(ITimer::now())) {
        for (const Action &action : actions) {
            if (st.stop_requested()) {
                interrupted = true;
                break;
            }

            // delayMs = 上一条做完之后再等这么久，所以这里是纯前置等待。
            if (action.delayMs > 0 &&
                !timer.wait(rc.clampWait(ITimer::Duration{action.delayMs}, ITimer::now()), st)) {
                interrupted = true;
                break;
            }

            if (!applyAction(action, timer, st, held, heldButtons)) {
                interrupted = true;
                break;
            }

            ++executed;
        }

        if (interrupted) break;

        // 每轮结束补平，保证下一轮从"所有键都松开"开始。
        releaseHeldKeys(held);
        releaseHeldButtons(heldButtons);
        rc.noteIteration();

        if (st.stop_requested()) break;
    }

    // 退出兜底：被 stop 打断时可能正按着键/鼠标键
    releaseHeldKeys(held);
    releaseHeldButtons(heldButtons);

    LOG_DEBUG("playback: run finished, executed={}", executed);
    return executed;
}

} // namespace playback
