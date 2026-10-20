#include "win32/Input.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <windows.h>

#include "common/Log.hpp"

// ============================================================
// 实现要点
//
// 坐标：SendInput 的绝对坐标是 0–65535 的归一化值，配合
//   MOUSEEVENTF_VIRTUALDESK 映射到整个虚拟桌面（多显示器、负坐标都算在内），
//   所以先用 SM_X/YVIRTUALSCREEN + SM_CX/CYVIRTUALSCREEN 换算。
//   分母取"宽 - 1"：65535 对应最后一个像素；用"宽"的话最右/最下一列会落到
//   屏幕外点不中。MulDiv 带四舍五入，比整数除法准半个像素。
//
// 键盘：只填 wVk，不填 wScan、不加 KEYEVENTF_EXTENDEDKEY。系统会自己把 VK
//   翻成扫描码；手工填扫描码却不加扩展前缀，反而会让读扫描码的程序认错键。
//   真遇到某个程序不认（老游戏之类）再针对性补。
//
// 标记：所有事件都带 dwExtraInfo = kInjectedSignature。Recorder 的低级钩子会
//   收到我们自己注入的事件（回放时尤其明显），钩子靠这个值过滤掉，否则
//   边回放边录制会自己录自己。
// ============================================================

namespace {
    // 'KBMR'。注入事件的身份标记，Recorder 的钩子用同一个值过滤。
    constexpr ULONG_PTR kInjectedSignature = 0x4B424D52u;

    // 干跑模式：打开 ENABLE_DRY_RUN 时只记日志、不调 SendInput——
    // 连那次调用都不会编进去，所以对整个系统零影响。
    // 除"发不发"之外，行为与真实模式完全一致：照样等 holdMs、照样记日志、
    // 照样返回 true，日志格式只多一个 dry 标记。
#ifdef KBMACRO_DRY_RUN
    constexpr bool kDryRun = true;
#else
    constexpr bool kDryRun = false;
#endif

    // 修饰键的按下顺序：Ctrl → Shift → Alt → Win；抬起时反过来。
    struct ModifierKey {
        unsigned mask;
        WORD vk;
    };

    constexpr ModifierKey kModifierKeys[] = {
        {.mask = hk::kModCtrl,  .vk = VK_CONTROL},
        {.mask = hk::kModShift, .vk = VK_SHIFT},
        {.mask = hk::kModAlt,   .vk = VK_MENU},
        {.mask = hk::kModWin,   .vk = VK_LWIN},
    };

    // 合成一个鼠标事件。结构体必须整体清零（值初始化）。
    [[nodiscard]] INPUT makeMouse(const DWORD flags, const LONG dx = 0, const LONG dy = 0,
                                  const DWORD data = 0) {
        INPUT in{};
        in.type           = INPUT_MOUSE;
        in.mi.dx          = dx;
        in.mi.dy          = dy;
        in.mi.mouseData   = data;
        in.mi.dwFlags     = flags;
        in.mi.dwExtraInfo = kInjectedSignature;
        return in;
    }

    [[nodiscard]] INPUT makeKeyboard(const WORD vk, const DWORD flags) {
        INPUT in{};
        in.type           = INPUT_KEYBOARD;
        in.ki.wVk         = vk;
        in.ki.dwFlags     = flags;
        in.ki.dwExtraInfo = kInjectedSignature;
        return in;
    }

    // 每个鼠标键对应的 按下/抬起 标志，以及侧键编号。
    struct ButtonFlags {
        DWORD down;
        DWORD up;
        DWORD data;
    };

    [[nodiscard]] constexpr ButtonFlags buttonFlags(const MouseButton button) noexcept {
        switch (button) {
            case MouseButton::Left:
                return {.down = MOUSEEVENTF_LEFTDOWN, .up = MOUSEEVENTF_LEFTUP, .data = 0};
            case MouseButton::Right:
                return {.down = MOUSEEVENTF_RIGHTDOWN, .up = MOUSEEVENTF_RIGHTUP, .data = 0};
            case MouseButton::Middle:
                return {.down = MOUSEEVENTF_MIDDLEDOWN, .up = MOUSEEVENTF_MIDDLEUP, .data = 0};
            case MouseButton::Side1:
                return {.down = MOUSEEVENTF_XDOWN, .up = MOUSEEVENTF_XUP, .data = XBUTTON1};
            case MouseButton::Side2:
                return {.down = MOUSEEVENTF_XDOWN, .up = MOUSEEVENTF_XUP, .data = XBUTTON2};
        }
        return {.down = MOUSEEVENTF_LEFTDOWN, .up = MOUSEEVENTF_LEFTUP, .data = 0};
    }

    // 键盘事件序列：按下时修饰键在前、主键在后；抬起时主键在前、修饰键在后。
    // 主键未设置（vk == 0）时只发修饰键；两者都没有就返回空表。
    [[nodiscard]] std::vector<INPUT> keyEvents(const Hotkey &hk, const bool down) {
        const DWORD flags = down ? 0u : KEYEVENTF_KEYUP;

        std::vector<INPUT> modifiers;
        for (const auto &[mask, vk]: kModifierKeys) {
            if ((hk.modifiers & mask) != 0) {
                modifiers.push_back(makeKeyboard(vk, flags));
            }
        }

        std::vector<INPUT> main;
        if (hk.vk != 0) {
            main.push_back(makeKeyboard(static_cast<WORD>(hk.vk), flags));
        }

        if (down) {
            modifiers.insert(modifiers.end(), main.begin(), main.end());
            return modifiers;
        }
        main.insert(main.end(), modifiers.begin(), modifiers.end());
        return main;
    }

    // 日志里描述一个按键组合。
    [[nodiscard]] std::string keyLabel(const Hotkey &hk) {
        return "vk=" + std::to_string(hk.vk) + " mods=" + std::to_string(hk.modifiers);
    }

    // 发一组事件：一次 SendInput = 一行 DEBUG。
    // 失败（返回值小于预期）→ 一行 WARN → 补发 cleanup（抬起类事件）→ 抛。
    //   what    : 日志与异常里的动作描述
    //   cleanup : 失败后补发的事件；nullptr = 无需收拾现场（移动、滚轮）
    void sendEvents(std::vector<INPUT> events, std::string what,
                    std::vector<INPUT> *cleanup = nullptr) {
        if (events.empty()) {
            LOG_DEBUG("input: {} skipped, no events", what);
            return;
        }

        const auto expected = static_cast<UINT>(events.size());

        // 干跑：不调 SendInput，直接认为全部插入成功
        UINT sent = expected;
        if constexpr (!kDryRun) {
            sent = ::SendInput(expected, events.data(), static_cast<int>(sizeof(INPUT)));
        }

        if (sent == expected) {
            LOG_DEBUG("input: {} (sent={}/{}, dry={})", what, sent, expected,
                      static_cast<int>(kDryRun));
            return;
        }

        // 插入失败基本只有两种原因：前台窗口完整性更高（UIPI 挡掉注入）、
        // 或者 BlockInput 生效。两者都是持续性的，重试没有意义。
        const DWORD error = ::GetLastError();
        LOG_WARN("input: {} failed: sent={}/{}, GetLastError={}", what, sent, expected, error);

        if (cleanup != nullptr && !cleanup->empty()) {
            const auto cleanupCount = static_cast<UINT>(cleanup->size());
            const UINT cleaned =
                ::SendInput(cleanupCount, cleanup->data(), static_cast<int>(sizeof(INPUT)));
            if (cleaned != cleanupCount) {
                LOG_ERROR("input: {} cleanup failed: sent={}/{}, GetLastError={}", what, cleaned,
                          cleanupCount, ::GetLastError());
            }
        }

        throw input::SendInputError(what + " failed: sent=" + std::to_string(sent) + "/" +
                                    std::to_string(expected) +
                                    ", GetLastError=" + std::to_string(error));
    }
} // namespace

namespace input {

void init() {
    const int x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

    int dpi = 0;
    if (const HDC dc = ::GetDC(nullptr); dc != nullptr) { // NOLINT(*-misplaced-const)
        dpi = ::GetDeviceCaps(dc, LOGPIXELSX);
        ::ReleaseDC(nullptr, dc);
    }

    LOG_DEBUG("input: virtual screen at ({}, {}), {}x{}, dpi={}", x, y, w, h, dpi);

    if constexpr (kDryRun) {
        LOG_WARN("input: DRY RUN enabled, nothing will be injected");
    }
}

void moveTo(const int x, const int y) {
    const int screenX = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int screenY = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int screenW = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int screenH = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // 65535 对应最后一个像素 → 分母是 宽/高 - 1；1 像素宽的退化情况用 1 兜底。
    const int spanW = (screenW > 1) ? screenW - 1 : 1;
    const int spanH = (screenH > 1) ? screenH - 1 : 1;

    const int normX = ::MulDiv(x - screenX, 65535, spanW);
    const int normY = ::MulDiv(y - screenY, 65535, spanH);

    std::string what = "move to (" + std::to_string(x) + ", " + std::to_string(y) + ")";
    if constexpr (kDryRun) {
        // 干跑时把归一化结果也记出来：排查"坐标偏"不用再手推公式
        what += " norm=(" + std::to_string(normX) + ", " + std::to_string(normY) + ")";
    }

    sendEvents({makeMouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
                          normX, normY)},
               std::move(what));
}

void buttonDown(const MouseButton button) {
    const auto [down, up, data] = buttonFlags(button);
    // 按下失败时补一次抬起：无论是完全没按下去还是只落了一部分，抬起都无害。
    std::vector<INPUT> cleanup{makeMouse(up, 0, 0, data)};
    sendEvents({makeMouse(down, 0, 0, data)},
               std::string(mouseButtonToString(button)) + " down", &cleanup);
}

void buttonUp(const MouseButton button) {
    const ButtonFlags flags = buttonFlags(button);
    std::vector<INPUT> retry{makeMouse(flags.up, 0, 0, flags.data)};
    sendEvents({makeMouse(flags.up, 0, 0, flags.data)},
               std::string(mouseButtonToString(button)) + " up", &retry);
}

void wheel(const WheelDirection direction, const int steps) {
    const bool vertical = (direction == WheelDirection::Up || direction == WheelDirection::Down);
    const bool negative = (direction == WheelDirection::Down || direction == WheelDirection::Left);
    const LONG delta    = steps * WHEEL_DELTA;

    // 纵向用 MOUSEEVENTF_WHEEL（正 = 上滚），横向用 MOUSEEVENTF_HWHEEL（正 = 右滚）。
    // 多步累加进一条事件的 mouseData，一次 SendInput 一行日志。
    sendEvents({makeMouse(vertical ? MOUSEEVENTF_WHEEL : MOUSEEVENTF_HWHEEL, 0, 0,
                          static_cast<DWORD>(negative ? -delta : delta))},
               std::string(wheelDirectionToString(direction)) + " x" + std::to_string(steps));
}

void keyDown(const Hotkey &hk) {
    std::vector<INPUT> events = keyEvents(hk, true);
    std::vector<INPUT> cleanup = keyEvents(hk, false);
    sendEvents(std::move(events), "key down " + keyLabel(hk), &cleanup);
}

void keyUp(const Hotkey &hk) {
    std::vector<INPUT> events = keyEvents(hk, false);
    std::vector<INPUT> retry  = events;   // 抬起失败就再补一次抬起
    sendEvents(std::move(events), "key up " + keyLabel(hk), &retry);
}

bool click(const MouseButton button, const int holdMs, const ITimer &timer,
           const std::stop_token &st) {
    if (st.stop_requested()) return false;

    buttonDown(button);
    // 按住时长走计时器。被打断也要抬起：不能把鼠标键留在按下状态。
    const bool completed = timer.wait(ITimer::Duration{holdMs}, st);
    buttonUp(button);
    return completed;
}

bool pressKey(const Hotkey &hk, const int holdMs, const ITimer &timer, const std::stop_token &st) {
    if (st.stop_requested()) return false;

    keyDown(hk);
    const bool completed = timer.wait(ITimer::Duration{holdMs}, st);
    keyUp(hk);
    return completed;
}

} // namespace input
