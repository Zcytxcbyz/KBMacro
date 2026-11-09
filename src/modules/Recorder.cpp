#include "modules/Recorder.hpp"

#include <windows.h>

#include <wil/resource.h>

#include <algorithm>
#include <array>
// ReSharper disable once CppUnusedIncludeDirective
#include <cstddef>
#include <cstdint>
#include <moodycamel/concurrentqueue.h>
#include <optional>
#include <vector>

#include "common/Log.hpp"
#include "common/RateLimiter.hpp"
#include "modules/RecorderNormalize.hpp"
#include "win32/Input.hpp"

// ============================================================================
// 录制模块：两个低级钩子采集键鼠事件，归一化成 Action 序列。
//
// 线程模型（除了两端的 UI 调用，全部在录制线程里完成）：
//
//   UI 线程                          录制线程
//   ───────                          ────────
//   StartRecord(opts, cb) ────────→ run()
//                                      ├ 开始延迟（timer.wait，可被打断）
//                                      ├ PeekMessage(PM_NOREMOVE)  建消息队列
//                                      ├ SetWindowsHookEx ×2       装钩子
//                                      ├ stop_callback             停止时叫醒我
//                                      └ 泵循环 ←── 系统在这里调用回调
//   StopRecord() ─── request_stop ─────┘
//                └── join 等待收尾
//
// 两条贯穿全文件的纪律：
//
//   【纪律 1】钩子回调是热路径，只许"抄参数 + 入队 + 放行"。
//     低级钩子直接挂在系统输入链上，系统给每次回调 300ms
//     （LowLevelHooksTimeout，注册表默认值），超时就静默摘掉钩子、之后再也
//     收不到事件。所以回调里没有过滤、没有采样、没有日志、没有任何分配。
//
//   【纪律 2】判断、过滤、翻译全部放在泵里。
//     泵跑在自己的线程里，不受 300ms 约束，还能批量取 256 条（省原子操作）。
//     代价是"不想要的事件"也要先进队列，所以容量按最坏情况给到 65536。
// ============================================================================

namespace {

    // ─────────────────────────────── 常量 ───────────────────────────────

    // 停止信号。WM_APP 以上是应用自有区间，不会与系统消息冲突。
    //   由 stop_callback 里的 PostThreadMessage 发出，用来把阻塞在等待里的
    //   泵立刻叫醒（而不是等下一个 1 秒节拍）。
    constexpr UINT kStopMessage = WM_APP + 0x4B;

    // 队列容量。因为回调不过滤，连不需要的移动事件也要先入队：
    //   125Hz 鼠标 → 125 条/秒；65536 条 ≈ 8.7 分钟连续移动。
    //   每条 RawHookEvent 约 48 字节，预分配约 3MB；一次性，之后回调里零分配。
    constexpr std::size_t kQueueCapacity = 65536;

    // 一次批量取多少条。批量取是为了减少队列的原子操作次数。
    constexpr std::size_t kDrainBatch = 256;

    // 进度日志的限频间隔（交给 RateLimiter，类本身不关心"多久"）。
    constexpr auto kProgressInterval = ITimer::Duration{1000};

    // 原料上限的提前停止系数。一条 Action 最多由 2 个原始事件产生
    //   （点击 = Down + Up；拖动还会多出中间的 Move，这里取最坏比 2）。
    //   精确截断仍然由 normalize 按"动作数"做，这里只是防止无限录下去。
    constexpr std::size_t kRawPerAction = 2;

    // 事件来源。键盘和鼠标的 msg 取值区间有重叠（都是 WM_xxx 0x0200 段），
    //   所以必须分开记，不能只看 msg。
    constexpr std::uint8_t kSourceKeyboard = 0;
    constexpr std::uint8_t kSourceMouse = 1;

    // ───────────────────────── 回调写进队列的记录 ─────────────────────────

    // 钩子结构体字段的逐字段拷贝，不做任何解释。
    //   为什么不直接存 KBDLLHOOKSTRUCT / MSLLHOOKSTRUCT？
    //   它们进同一个队列，必须能区分来源；而且把用不到的字段（如 scanCode）
    //   剔掉能让每条记录更小，队列装得更多。
    struct RawHookEvent {
        std::uint8_t source = kSourceKeyboard;

        std::uint32_t msg = 0;            // 原始 wParam：WM_KEYDOWN / WM_LBUTTONDOWN / WM_MOUSEWHEEL …
        std::uint32_t vk = 0;             // 键盘：vkCode
        std::int32_t x = 0;               // 鼠标：pt.x（虚拟桌面坐标）
        std::int32_t y = 0;               // 鼠标：pt.y
        std::uint32_t mouseData = 0;      // 滚轮：高 16 位是有符号 delta；侧键：高 16 位是 XBUTTON1/2
        std::uint32_t flags = 0;          // LLKHF_INJECTED / LLMHF_INJECTED
        std::uintptr_t extraInfo = 0;     // 注入标记：我们自己发的会带 'KBMR'
        std::uint32_t time = 0;           // 钩子结构体里的 time（GetTickCount 毫秒，会回绕）
    };

    // 回调与泵之间的唯一通道。
    //   thread_local：钩子必须装在有它的线程上，回调也一定在同一条线程里被调用，
    //   因此这里不需要任何锁、原子变量或内存屏障。
    thread_local moodycamel::ConcurrentQueue<RawHookEvent> *t_queue = nullptr;

    // 入队失败的次数。这是回调里唯一允许写的东西，因为"入队成功与否"只有
    //   回调自己能看到（泵看不到丢掉的那几条）；它不对事件做任何判断，
    //   只是记录一次客观失败，供退出时的日志使用。
    thread_local std::size_t t_droppedQueueFull = 0;

    // 离开作用域时把 t_queue 置空：即使 run() 被异常掀翻，也不会留下
    //   指向已析构队列的悬垂指针继续被系统回调使用。
    struct QueueGuard {
        ~QueueGuard() { t_queue = nullptr; }
    };

    // ─────────────────── 钩子回调（热路径：只抄参数 + 入队） ───────────────────

    // 键盘低级钩子。注意：
    //   - nCode 的检查是 API 协议，不是对事件的判断（nCode < 0 时结构体无效）
    //   - 无论丢不丢，都必须 CallNextHookEx，否则前台程序收不到这次按键
    //   - 耗时预算：48 字节拷贝 + 一次 try_enqueue ≈ 几十纳秒
    LRESULT CALLBACK onKeyboard(const int nCode, const WPARAM wParam, const LPARAM lParam) {
        if (nCode == HC_ACTION && t_queue != nullptr) {
            const auto *data = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);

            RawHookEvent e;
            e.source = kSourceKeyboard;
            e.msg = static_cast<std::uint32_t>(wParam);
            e.vk = data->vkCode;
            e.flags = data->flags;
            e.extraInfo = data->dwExtraInfo;
            e.time = data->time;

            // 队列满：丢这条，不重试、不等待。
            //   录制出现缺口，好过让系统输入卡在钩子链上。
            if (!t_queue->try_enqueue(e)) ++t_droppedQueueFull;
        }
        return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // 鼠标低级钩子，结构与键盘完全对称。
    LRESULT CALLBACK onMouse(const int nCode, const WPARAM wParam, const LPARAM lParam) {
        if (nCode == HC_ACTION && t_queue != nullptr) {
            const auto *data = reinterpret_cast<const MSLLHOOKSTRUCT *>(lParam);

            RawHookEvent e;
            e.source = kSourceMouse;
            e.msg = static_cast<std::uint32_t>(wParam);
            e.x = data->pt.x;
            e.y = data->pt.y;
            e.mouseData = data->mouseData;
            e.flags = data->flags;
            e.extraInfo = data->dwExtraInfo;
            e.time = data->time;

            if (!t_queue->try_enqueue(e)) ++t_droppedQueueFull;
        }
        return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // ───────────────────────────── 泵侧的状态 ─────────────────────────────

    // 五类丢弃的计数。只为了退出时写进日志——排查"为什么没录到"时，
    //   光看 raw/actions 两个数字看不出是被谁吃掉的。
    struct Dropped {
        std::size_t injected = 0;  // 自家注入（'KBMR'）或用户选择滤掉的注入
        std::size_t packet = 0;    // VK_PACKET（输入法合成，回放不了）
        std::size_t hotkey = 0;    // 开始/停止热键的主键
        std::size_t sampled = 0;   // 被移动采样丢掉的
    };

    struct CaptureState {
        const recorder::RecordOptions &opts;

        std::vector<recorder::RawEvent> raw;  // 过滤翻译后的原料，最后交给 normalize
        Dropped dropped;

        // 上一次"被接受"的鼠标移动时间。只在接受时推进——否则慢速拖动会因为
        //   每一步都不足采样间隔而被全部吃掉。
        std::uint32_t lastMoveTime = 0;
    };

    // 侧键：mouseData 的高 16 位是 XBUTTON1 / XBUTTON2。
    [[nodiscard]] MouseButton buttonOf(const RawHookEvent &e) noexcept {
        switch (e.msg) {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                return MouseButton::Left;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return MouseButton::Right;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return MouseButton::Middle;
            default:
                break;
        }
        return (HIWORD(e.mouseData) == XBUTTON2) ? MouseButton::Side2 : MouseButton::Side1;
    }

    // 判定表：返回 false 表示这条不要，丢弃原因计入计数。
    //   顺序是刻意的——前三条与用户配置无关（硬性），所以排在开关前面。
    [[nodiscard]] bool accept(CaptureState &state, const RawHookEvent &e) {
        const recorder::RecordOptions &opts = state.opts;
        const bool keyboard = (e.source == kSourceKeyboard);

        // 1) 我们自己注入的事件：永远滤掉。不滤的话"边回放边录制"会自己录自己。
        if (e.extraInfo == input::kInjectedSignature) {
            ++state.dropped.injected;
            return false;
        }

        // 2) VK_PACKET：输入法合成出来的按键，vk 里没有真实键码，回放不出来。
        if (keyboard && e.vk == VK_PACKET) {
            ++state.dropped.packet;
            return false;
        }

        // 3) 开始/停止热键的主键：它们只用来控制录制，产物里没有正当用途。
        //    注意只滤主键、不滤修饰键：停止热键若含 Ctrl，全程滤 Ctrl 会把用户
        //    正常按的 Ctrl+C 吃成"只按了个 C"。修饰键的残留由归一化阶段按
        //    "头尾连续段"剔除。
        if (keyboard && e.vk != 0 &&
            ((opts.startKeyVk != 0 && e.vk == opts.startKeyVk) ||
             (opts.stopKeyVk != 0 && e.vk == opts.stopKeyVk))) {
            ++state.dropped.hotkey;
            return false;
        }

        // 4) 其它程序注入的事件：用户可选。默认不滤，因为"录不到"比"录到脏数据"
        //    更难排查。键盘与鼠标的注入标志是两个不同的位，按来源取。
        const std::uint32_t injectedFlag = keyboard ? LLKHF_INJECTED : LLMHF_INJECTED;
        if (opts.ignoreInjected && (e.flags & injectedFlag) != 0) {
            ++state.dropped.injected;
            return false;
        }

        // 5) 鼠标移动采样：距上一次"被接受"的移动不足间隔就丢掉。
        //    time 会回绕，所以必须用无符号差值比较（不能用 < 直接比大小）。
        if (!keyboard && e.msg == WM_MOUSEMOVE) {
            const auto interval = static_cast<std::uint32_t>(std::max(0, opts.moveIntervalMs));
            if (interval > 0 && (e.time - state.lastMoveTime) < interval) {
                ++state.dropped.sampled;
                return false;
            }
            state.lastMoveTime = e.time;
        }

        return true;
    }

    // 翻译表：RawHookEvent → 归一化用的 RawEvent。返回空表示这条不认识（丢弃）。
    [[nodiscard]] std::optional<recorder::RawEvent> translate(const RawHookEvent &e) {
        recorder::RawEvent r;
        r.time = e.time;

        if (e.source == kSourceKeyboard) {
            if (e.msg == WM_KEYDOWN || e.msg == WM_SYSKEYDOWN) {
                r.kind = recorder::RawKind::KeyDown;
            } else if (e.msg == WM_KEYUP || e.msg == WM_SYSKEYUP) {
                r.kind = recorder::RawKind::KeyUp;
            } else {
                return std::nullopt;
            }
            r.vk = e.vk;
            return r;
        }

        switch (e.msg) {
            case WM_MOUSEMOVE:
                r.kind = recorder::RawKind::Move;
                break;
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
                r.kind = recorder::RawKind::ButtonDown;
                r.button = buttonOf(e);
                break;
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_XBUTTONUP:
                r.kind = recorder::RawKind::ButtonUp;
                r.button = buttonOf(e);
                break;
            case WM_MOUSEWHEEL:
                r.kind = recorder::RawKind::Wheel;
                r.wheelDelta = static_cast<std::int16_t>(HIWORD(e.mouseData));  // 有符号
                r.horizontal = false;
                break;
            case WM_MOUSEHWHEEL:
                r.kind = recorder::RawKind::Wheel;
                r.wheelDelta = static_cast<std::int16_t>(HIWORD(e.mouseData));
                r.horizontal = true;
                break;
            default:
                return std::nullopt;
        }

        r.x = e.x;
        r.y = e.y;
        return r;
    }

    // ─────────────────────────────── 泵 ───────────────────────────────

    // 把队列里现存的事件全部搬出来：批量取 → 判定 → 翻译 → 追加。
    //   返回 true 表示原料数已达早停阈值（调用方据此退出）。
    bool drainInto(CaptureState &state, moodycamel::ConcurrentQueue<RawHookEvent> &queue) {
        static thread_local std::array<RawHookEvent, kDrainBatch> batch{};

        std::size_t got = 0;
        while ((got = queue.try_dequeue_bulk(batch.data(), batch.size())) > 0) {
            for (std::size_t i = 0; i < got; ++i) {
                const RawHookEvent &e = batch[i];
                if (!accept(state, e)) continue;
                if (auto translated = translate(e)) state.raw.push_back(*translated);

                if (state.opts.maxActions > 0 &&
                    state.raw.size() >= kRawPerAction * static_cast<std::size_t>(state.opts.maxActions)) {
                    return true;
                }
            }
        }
        return false;
    }

    // 泵主循环。退出条件只有三个：收到停止信号、达到原料上限、等待失败。
    void pumpLoop(CaptureState &state, moodycamel::ConcurrentQueue<RawHookEvent> &queue,
                  const std::stop_token &st) {
        RateLimiter progress{kProgressInterval};
        const auto started = ITimer::now();

        MSG msg{};
        bool limitReached = false;

        while (true) {
            // ① 先把队列搬空（过滤/翻译都在这里，不在回调里）
            limitReached = drainInto(state, queue);

            // ② 进度：RateLimiter 保证每秒最多一条；首次调用立即放行
            if (progress.allow()) {
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::seconds>(ITimer::now() - started).count();
                LOG_INFO("recorder: recording... {}s, raw={}, dropped injected={} packet={} "
                         "hotkey={} sampled={} queueFull={}",
                         elapsed, state.raw.size(), state.dropped.injected, state.dropped.packet,
                         state.dropped.hotkey, state.dropped.sampled, t_droppedQueueFull);
            }

            // ③ 达到原料上限：精确截断留给 normalize
            if (limitReached) {
                LOG_WARN("recorder: action limit reached");
                break;
            }
            // ④ 已经有人请求停止（stop_callback 之外的第二道保险）
            if (st.stop_requested()) break;

            // ⑤ 等输入：有消息立刻返回（≈0 延迟），没有就 1 秒后返回打进度
            const DWORD waited = ::MsgWaitForMultipleObjects(
                0, nullptr, FALSE, static_cast<DWORD>(kProgressInterval.count()), QS_ALLINPUT);

            if (waited == WAIT_OBJECT_0) {
                // ⑥ 取出消息。低级钩子的回调就是在这里面被系统调用的
                //    （PeekMessage 内部派发），所以这一步本身就是"收输入"。
                bool stopMessage = false;
                while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    if (msg.message == kStopMessage) {
                        stopMessage = true;
                        break;
                    }
                }
                if (stopMessage) break;
            } else if (waited == WAIT_FAILED) {
                // 不应该发生；真发生了当停止处理，至少不会无限转下去
                LOG_WARN("recorder: message wait failed");
                break;
            }
            // WAIT_TIMEOUT：什么都不做，回到 ①（顺便到了下一次进度的时间点）
        }

        // 退出前再搬一次：停止信号到达与最后一次入队之间可能还有残余
        static_cast<void>(drainInto(state, queue));
    }

    [[nodiscard]] recorder::NormalizeOptions makeNormalizeOptions(const recorder::RecordOptions &opts) {
        recorder::NormalizeOptions n;
        n.startKeyVk = opts.startKeyVk;
        n.stopKeyVk = opts.stopKeyVk;
        n.startMods = opts.startMods;
        n.stopMods = opts.stopMods;
        n.maxActions = opts.maxActions;
        return n;
    }

} // namespace

// ============================================================================
// 对外入口
// ============================================================================

std::vector<Action> recorder::run(const RecordOptions &opts, const ITimer &timer,
                                  const std::stop_token &st) {
    // ── 1. 开始延迟 ─────────────────────────────────────────────────────
    // 位置在装钩子之前：这段等待里还没有任何输入被我们拦着，晚几毫秒无所谓；
    // 反过来如果先装钩子再等，就等于"前 200ms 什么都不录，但系统已经被拦着"。
    // 主要目的是让开始热键那一组按键先抬起来，头尾剔除是第二道保险。
    if (const int startDelayMs = std::max(0, opts.startDelayMs); startDelayMs > 0) {
        if (!timer.wait(ITimer::Duration{startDelayMs}, st)) {
            LOG_DEBUG("recorder: stopped during start delay");
            return {};
        }
    }

    // ── 2. 先让本线程拥有消息队列 ────────────────────────────────────────
    // Windows 的线程消息队列是懒创建的；没有队列时 PostThreadMessage 会失败，
    // 停止信号就永远发不出去，StopRecord 会一直卡在 join 上。
    // 这里用"最小的代价"把它摸出来：只看 WM_USER 这一个值、且不取走消息。
    MSG msg{};
    ::PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    // ── 3. 队列与回调入口 ───────────────────────────────────────────────
    moodycamel::ConcurrentQueue<RawHookEvent> queue{kQueueCapacity};  // 预分配，回调里零分配
    t_droppedQueueFull = 0;
    t_queue = &queue;
    QueueGuard queueGuard;  // 任何路径离开本函数都会把 t_queue 置空

    // ── 4. 装两个钩子 ──────────────────────────────────────────────────
    // 顺序构造：第二个失败时第一个已经由 wil::unique_hhook 自动卸掉，
    //   "要么都装上、要么都不装"是结构上成立的，不需要手写回滚。
    // hMod 传自身 EXE 的句柄；低级钩子的回调在我们自己的线程里执行，
    //   所以不需要（也没有）额外的 DLL。
    const DWORD threadId = ::GetCurrentThreadId();

    wil::unique_hhook keyboardHook{
        ::SetWindowsHookExW(WH_KEYBOARD_LL, &onKeyboard, ::GetModuleHandleW(nullptr), 0)};
    THROW_LAST_ERROR_IF_NULL(keyboardHook);

    wil::unique_hhook mouseHook{
        ::SetWindowsHookExW(WH_MOUSE_LL, &onMouse, ::GetModuleHandleW(nullptr), 0)};
    THROW_LAST_ERROR_IF_NULL(mouseHook);

    // ── 5. 停止回调：把阻塞在等待里的泵叫醒 ──────────────────────────────
    // request_stop 可能在任意时刻发生（UI 线程的 StopRecord），甚至就在注册的
    //   这一瞬间——std::stop_callback 在 stop 已被请求时会立即执行回调，
    //   于是这条消息会先于泵就躺在队列里，泵第一次拿消息就会看到它。
    std::stop_callback onStop{st, [threadId] {
        ::PostThreadMessageW(threadId, kStopMessage, 0, 0);
    }};

    LOG_INFO("recorder: recording started (delay={}ms, moveInterval={}ms, ignoreInjected={}, "
             "maxActions={})",
             std::max(0, opts.startDelayMs), opts.moveIntervalMs, opts.ignoreInjected, opts.maxActions);

    // ── 6. 泵：等输入、搬队列、打进度，直到停止或达到上限 ─────────────────
    CaptureState state{opts};
    pumpLoop(state, queue, st);

    // ── 7. 收尾顺序（顺序本身就是正确性） ────────────────────────────────
    // ① 先断回调入口：从这一刻起即便还有回调进来，也不会往即将析构的队列里写
    // ② 再卸钩子：此后系统不再调用我们的回调
    // ③ 最后 drain 一次（在 pumpLoop 末尾已完成）：卸钩子前那一瞬间可能还有
    //    事件刚入队、泵还没取走；不做这一步会丢尾部
    t_queue = nullptr;
    mouseHook.reset();
    keyboardHook.reset();

    // ── 8. 归一化 ──────────────────────────────────────────────────────
    // 纯函数：头尾修饰键剔除、自动重复合并、点击/拖动判定、滚轮累积、
    //   delayMs 计算、maxActions 精确截断，全在这里完成（不在钩子回调里）。
    std::vector<Action> actions = normalize(state.raw, makeNormalizeOptions(opts));

    LOG_INFO("recorder: recording finished, raw={}, actions={}, dropped injected={} packet={} "
             "hotkey={} sampled={} queueFull={}",
             state.raw.size(), actions.size(), state.dropped.injected, state.dropped.packet,
             state.dropped.hotkey, state.dropped.sampled, t_droppedQueueFull);

    // 空表是合法结果："没录到动作"由面板决定提示什么
    return actions;
}
