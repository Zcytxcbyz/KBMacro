# Recorder 模块设计方案

> 状态：待批准
> 依赖：`ITimer` / `RunController` / `ThreadManager` / `win32::input` / `Playback` / `CoordAdapt`（均已落地）

## 1. 目标与范围

把「用户真实操作」录成 `std::vector<Action>`，产物与手写/导入的脚本同构，交给既有的
`Playback` 模块回放。

**范围内**：鼠标移动 / 左中右/侧键按下抬起 / 滚轮 / 键盘按下抬起；拖动；过滤与采样；
录制上限；交付弹窗。

**范围外**：中文输入法汉字（`VK_PACKET` 表达不了）、提权窗口内的输入（UIPI）、
录制过程的实时进度显示（backlog）。

## 2. 已拍板的决策

| # | 决策 | 理由 |
|---|---|---|
| 1 | 钩子装在**录制线程自身**，装配 `GetMessage` 之外的自建泵 | `StopXxx` 阻塞 join 期间若钩子在 UI 线程，回调延迟超过 `LowLevelHooksTimeout`(300ms) 会被系统静默卸载 |
| 2 | `ActionType` 增加 `MouseDown` / `MouseUp` | 现有 `MouseClick` 是原子的，无法表达"按住移动" |
| 3 | 鼠标移动**时间采样**，间隔为设置项（默认 16ms，0 = 不采样） | `WH_MOUSE_LL` 拿到的是设备原始上报，1000Hz 鼠标拖 10 秒约 1 万条 |
| 4 | 注入过滤为设置项（默认只滤自家 `'KBMR'`） | "要不要录 AHK 等工具的动作"是用户选择 |
| 5 | 开始/停止热键**永不进产物** | 程序要求的操作，副作用由程序消除 |
| 6 | 录制上限为设置项（默认 100000，0 = 不限） | 内存安全，且值由用户定 |

## 3. 数据模型

```cpp
// src/common/Action.hpp
enum class ActionType { MouseMove, MouseClick, MouseWheel, Keyboard, MouseDown, MouseUp };
// kActionTypeEntries 追加 {"MouseDown", "MouseUp"} → 序列化与表驱动字符串自动生效

// src/modules/Recorder.hpp
namespace recorder {

struct RecordOptions {
    int moveIntervalMs = 16;              // 0 = 不采样
    bool ignoreInjected = false;          // true = 连别的工具注入的也不录
    int startDelayMs = 200;               // 开始录制后先等这么久再采集
    int maxActions = 100000;              // 0 = 不限
    unsigned startKeyVk = 0;              // 开始热键主键（0 = 未配置）
    unsigned stopKeyVk = 0;               // 停止热键主键
    unsigned startMods = 0;               // 开始热键修饰键位掩码
    unsigned stopMods = 0;                // 停止热键修饰键位掩码
};

[[nodiscard]] std::vector<Action> run(const RecordOptions &opts,
                                      const ITimer &timer,
                                      const std::stop_token &st);
}
```

## 4. 线程结构

```
run(opts, timer, st)
 ├─ PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE)   // 强制建立线程消息队列
 ├─ t_state = &state                               // thread_local，供静态回调
 ├─ kbHook  = unique_hhook{SetWindowsHookExW(WH_KEYBOARD_LL, &onKey,  GetModuleHandleW(nullptr), 0)}
 ├─ msHook  = unique_hhook{SetWindowsHookExW(WH_MOUSE_LL,    &onMouse, GetModuleHandleW(nullptr), 0)}
 │    └─ 失败 → THROW_LAST_ERROR_IF_NULL → 已有钩子被 RAII 卸掉（自动"全成或全失败"）
 ├─ LOG_INFO("recorder: started (sample={}ms, ignoreInjected={}, max={})")
 ├─ startDelay 等待（可被 stop 打断 → 直接跳到收尾）
 ├─ while (!st.stop_requested()) {
 │      timer.wait(10ms, st)          // 恒定节奏，被打断即退出
 │      drainQueue(state)             // 把原始事件搬进 state.events
 │      if (state.reachedLimit) break;
 │    }
 ├─ kbHook.reset(); msHook.reset();    // 显式提前卸钩子，此后不再收事件
 ├─ drainQueue(state)                  // 最后一次
 └─ return normalize(state.events, opts)   // 纯函数
```

- **`thread_local RawState *t_state`**：`HOOKPROC` 是静态函数、不能带捕获，状态只能这样暴露。
  钩子装与卸都在同一线程、回调也只在该线程触发，无需任何原子或锁。
- **泵用 `PeekMessage` + `timer.wait(10ms)` 而不是阻塞 `GetMessage`**：节奏恒定、代码简单，
  空闲时每 10ms 醒一次（开销极低）；停止响应 ≤ 20ms（Sleep 计时器）。
- **`ThreadManager` 侧**：`StartRecord(const RecordOptions &, FinishCallback)`，
  body 里 `return TaskResult{.recorded = std::make_shared<std::vector<Action>>(recorder::run(...))};`，
  `executed` 恒 0。

## 5. 过滤与采样（钩子回调内，无锁、无日志、不做换算）

判定顺序，命中即 `return CallNextHookEx(...)`：

| 序 | 条件 | 动作 |
|---|---|---|
| 1 | `dwExtraInfo == 0x4B424D52`（自家注入） | 丢弃 |
| 2 | `vk == VK_PACKET (0xE7)` | 丢弃，`++unsupported` |
| 3 | `vk == opts.startKeyVk` 或 `vk == opts.stopKeyVk`（非 0 时） | 丢弃 |
| 4 | `opts.ignoreInjected && (flags & LLKHF_INJECTED / LLMHF_INJECTED)` | 丢弃 |
| 5 | 鼠标移动且 `time - lastMoveTime < moveIntervalMs` | 丢弃 |
| 6 | 其它 | `queue.try_enqueue(raw)`（失败 `++dropped`） |

`ConcurrentQueue<RawEvent>` 预分配 8192 项。

## 6. 归一化（纯函数，可单测）

`normalize(const std::vector<RawEvent> &, const RecordOptions &) -> std::vector<Action>`

1. **键盘事件 `modifiers` 恒为 0**。和弦靠事件序列表达（`Ctrl↓ C↓ C↑ Ctrl↑`）。
   若给 `C` 标上 `modifiers=Ctrl`，回放时 `keyDown` 会重复发 `Ctrl↓`、`keyUp` 会提前松 `Ctrl`，
   多键同按立刻坏掉。
2. **自动重复合并**：同一 `vk` 在收到 `KeyUp` 之前再次 `KeyDown` → 丢弃。
3. **点击 vs 拖动**：`MouseDown` 先挂起；后续出现位移 > 2px 的 `Move` → 先吐 `MouseDown`，
   再吐该 `Move`；到 `MouseUp` 时若未超阈值 → 吐一条 `MouseClick{holdMs = up.time - down.time}`，
   否则吐 `MouseUp`。
4. **`MouseDown`/`MouseUp` 带 `x/y`**，回放侧先 `moveTo` 再按/松（拖动起终点精确）。
5. **滚轮累积**：`pending += wheelDelta`；`|pending| >= 120` 时吐 `MouseWheel{steps = pending/120}`
   并扣掉，余数留到下一批。触摸板的细碎滚动因此能还原。
6. **`delayMs` = 本事件时间 − 上一条已产出动作的事件时间**，第一条为 0；用原始事件时间。
7. **头尾段剔除**：头段砍连续的"开始热键修饰键"事件，尾段砍连续的"停止热键修饰键"事件，
   遇到不属于该集合的动作立即停止扫描。
8. **`maxActions`**：累计达到上限时停止录制（`state.reachedLimit = true`），保留已录内容。
9. 产物为空时返回空 vector，面板据此提示"没有录到动作"。

时间戳：`DWORD` 毫秒（`GetTickCount` 时基，49.7 天回绕），一律用无符号差值计算。

## 7. 停止与交付

```
停止热键 / 按钮 → ThreadManager::StopRecord → request_stop
  → timer.wait(10ms) 返回 false → 卸钩子 → 最后一次 drain → normalize
  → TaskResult{.recorded} → launch 里算 reason（stop_requested ? Stopped : Completed）
  → CallAfter → 面板回调：
       recorded 为空        → 提示"没有录到动作"，不动 m_actions
       recorded 非空        → 弹窗「替换 / 追加 / 取消」
  → setRecording(false)
```

录制与回放互斥由 `ThreadManager` 保证（`StartRecord` 在回放运行中直接拒绝并 WARN）。

## 8. 配置与 UI

设置页新增「录制」小节：

| 标签 | 字段 | 范围 | 默认 |
|---|---|---|---|
| 鼠标移动最小间隔 (ms) | `recordMoveIntervalMs` | 0–1000 | 16 |
| ☐ 忽略模拟输入（只录真人操作） | `recordIgnoreInjected` | — | 关 |
| 录制开始延迟 (ms) | `recordStartDelayMs` | 0–2000 | 200 |
| 录制动作上限（0 = 不限） | `recordMaxActions` | 0–1000000 | 100000 |

四项均"下一次录制生效"，不写入 `*.macro.json`。
`HotkeyManager` 新增两个只读查询，供面板组装 `RecordOptions`：开始/停止热键的 `vk` 与 `modifiers`。

## 9. 测试计划

`tests/test_recorder_normalize.cpp`（纯函数、无 Win32 依赖）：

- 键盘：和弦序列不带 modifiers / 自动重复合并 / 按下抬起 / 被排除的按键不出现
- 鼠标：原地点击压成一条且 holdMs 正确 / 阈值内抖动仍算点击 / 拖动产出 Down+Move…+Up /
  拖动终点坐标 / 多按钮
- 滚轮：累积到 120 才产出 / 余数保留
- 时序：delayMs 差值 / 时间戳回绕 / 第一条 delay 为 0
- 头尾剔除：两条

**不测**：钩子本身、消息泵、`SetWindowsHookEx`。

## 10. 已知边界

1. 中文输入法的汉字录不到（`VK_PACKET` 丢弃），只有物理按键。
2. 提权窗口内的输入看不到（UIPI）。
3. 产物里的 `screen` 取导出时的 `GetPrimaryScreenSpec()`，非录制时的快照。
4. 钩子被系统静默卸载时表现为"录着录着不录了"，暂无检测。

## 11. 交付批次

| 批 | 内容 | 验收 |
|---|---|---|
| 1 | `ActionType` +2、`Playback` +2 case + 鼠标补平、4 条测试 | 现有 137 例 + 新增全绿 |
| 2 | `RecorderNormalize.hpp/.cpp` + 16 条单测 | 纯逻辑全绿 |
| 3 | `Config` 4 字段 + 4 个 setter + 序列化测试 | 往返 + clamp |
| 4 | `Recorder.hpp/.cpp` + `ThreadManager::StartRecord` 改造 | 能编、能录 |
| 5 | 面板接线 + 交付弹窗 + 设置页 + i18n | 端到端可用 |
| 6 | 冒烟：点击 / 拖动 / 滚轮 / 组合键 / 热键不进产物 / 上限触发 | 逐项确认 |

## 12. 本次不做，但已在 backlog 排队

- **相对坐标**：`MouseMoveRelative` / `MouseClickRelative` 两个 `ActionType`，
  `x/y` 解释成 `dx/dy`。改动面只有枚举、表驱动字符串、回放 `applyAction` 三个分支，
  录制端继续产出绝对坐标。等有人真的需要"相对上一次位置移动"时再做。
- **录制实时进度显示**：目前只显示"录制中…"，将来由消费端每秒汇报一次动作数。
- **中文输入法文本捕获**：需要给 `Action` 加文本字段（不只 `vk + modifiers`），
  属于数据模型级改动，单独排期。

## 13. 明确不做

- 录制期间暂停 / 恢复（需求没出现，且会引入新的状态机）
- 多显示器逐屏录制（统一按虚拟桌面整体坐标录制，够用）
