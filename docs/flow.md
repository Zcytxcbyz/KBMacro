# KBMacro 运行流程（端到端，以连点器为例）

> 依据：commit `3df1d1e` 时的磁盘状态。
> 行号会随代码变动，**以函数名为准、行号只作定位参考**。
> 本文只描述已经存在的机制；标注 `TODO(接线)` 的部分尚未实现。

---

## 1. 组件与所有权

| 组件 | 位置 | 谁创建 / 谁持有 | 生命周期 |
|---|---|---|---|
| `ITimer` 实例 | `common/Timer.hpp` | `MainFrame::createTimer()` | 由 `MainFrame::m_timer`（`shared_ptr`）持有；切模式整体替换 |
| `ThreadManager` | `managers/ThreadManager.*` | `MainFrame`（**尚未接线**，计划 `unique_ptr`） | 随 `MainFrame` 构造/析构 |
| 工作线程 | `std::jthread` | `ThreadManager::Slot::thread` | 每次 Start 新建；自然结束后仍 `joinable`，等下次 Start 或 Stop/StopAll 回收 |
| 输入注入 | `win32/Input.cpp` | 无状态自由函数 | 进程级；`init()` 在 `MainFrame` 构造里调一次 |
| 面板 | `ui/panels/*` | `MainFrame` | 与窗口同生命周期 |

**声明顺序约束**：`MainFrame::m_timer` 必须声明在 `m_threadMgr` 之前 —— `ThreadManager` 持的是 `m_timer` 这个 `shared_ptr` 的**引用**，且析构时要先停线程再放计时器。

---

## 2. 启动链路（全部在 UI 线程）

```
按钮 / 热键
  └─ ClickerPanel::onStart              ClickerPanel.cpp:304
      └─ startWithCurrentParams()       ClickerPanel.cpp:313
          ├─ collectParams()  → ClickerParams（值拷贝）
          ├─ LOG_INFO clicker: start requested (...)        ← 用户意图日志
          └─ ThreadManager::StartClicker(p, cb)             ThreadManager.cpp:66
              └─ launch(kind, cb, body)                     ThreadManager.cpp:187
```

`launch` 内部的顺序（**不能调换**）：

| 顺序 | 动作 | 位置 |
|---|---|---|
| 1 | `state == Running` → `LOG_DEBUG` 忽略并返回（重复启动不重启） | :193-196 |
| 2 | `thread.joinable()` → `request_stop()` + `join()`（回收上一轮） | :202-207 |
| 3 | `const unsigned gen = ++generation;` ← **序号唯一推进点** | :211 |
| 4 | `state = Running` + `LOG_INFO task {} started: gen={}` | :213-214 |
| 5 | `cb` 装进 `shared_ptr`（`CallAfter` 会拷 functor） | :217 |
| 6 | 组装 `notify`（工作线程侧回调） | :221-237 |
| 7 | `slot.thread = std::jthread{...}` → 工作线程开始跑 | :240-259 |

两个**在 UI 线程完成**的捕获（很重要）：

- `timer = m_timerRef`：读一次 `m_timer`，`shared_ptr` 拷贝进闭包 → 运行中切计时器模式**不影响**本次任务；
- `p`（参数）：值捕获 → 启动后改控件不影响本次任务。

---

## 3. 工作线程内部（`clicker::run`，待实现）

```cpp
RunController rc(p.run, ITimer::now());
while (rc.shouldRun(ITimer::now())) {
    if (!p.ignorePosition) input::moveTo(p.x, p.y);          // 每轮先定位
    // 按 ClickType 发输入（见 clicker::toClickStep）：
    //   滚轮 → input::wheel(direction, p.wheelSteps)   不适用 holdMs
    //   按钮 → input::click(button, p.holdMs, timer, st)  返回 false = 被 stop 打断
    //   Double → times == 2，连做两次完整点击（决策：各等 holdMs）
    if (被打断) break;

    rc.noteIteration();
    const Duration gap{ 滚轮类 ? p.intervalMs : std::max(0, p.intervalMs - p.holdMs) };
    if (!timer->wait(rc.clampWait(gap, ITimer::now()), st)) break;   // 被 stop
}
return 已执行轮数;
```

约定：

- **循环体里不允许出现 `LOG_*`**（热路径）；可观测性由 `Input.cpp`「一次 SendInput 一行 DEBUG」提供。
- `input::click` 内部：按下 → `timer.wait(holdMs)` → 抬起；**按下失败会先补一次抬起再抛** `SendInputError`。
- 抛出的异常不在这里接，冒到 `launch` 的 `catch`。

---

## 4. 线程收尾与回到 UI

```cpp
// ThreadManager.cpp:244-257（工作线程）
try {
    res = body(st);                                                   // executed 在这里拿到
    res.reason = st.stop_requested() ? Stopped : Completed;           // 由 launch 判，body 填了也覆盖
} catch (...) { res = TaskResult{.reason = Failed}; }

notify(std::move(res));     // 恰好一次：正常 / 异常两条路各一次

// notify 内部（:221-237）：先记 finishing 日志，再 CallAfter 到 UI
m_ui.CallAfter([this, kind, gen, cb, res = std::move(res)]() mutable {
    Slot &s = m_slots[index(kind)];
    if (gen != s.generation) return;      // 陈旧通知，丢弃
    s.state = Stopped;                    // 先落状态
    if (*cb) (*cb)(std::move(res));       // 再回调
});
```

面板在 `cb` 里的职责：按 `reason` 选择状态栏文案、按 `executed` 报次数、恢复按钮状态。

---

## 5. 停止链路

```
ClickerPanel::onStop (:308) → stop() (:322) → ThreadManager::StopClicker (:86)
  → stopTask (:262):
      1. !thread.joinable() → LOG_DEBUG "stop ignored" → 返回
      2. request_stop() → join()          ← 阻塞 UI 线程
      3. state = Stopped + LOG_INFO "task {} stopped"
```

- 阻塞等待的上界：Sleep 计时器约 20ms（每 10ms 检查一次 stop），Spin 亚毫秒，Hybrid 介于两者之间。
- **为什么必须阻塞**：只有阻塞 join 才能兑现「点停止就是停下来了」（不出现 `Stopping` 状态）。
- 之后 worker 的「回音」仍会到达 UI：`gen` 未变 → 校验通过 → `state` 幂等置位 → `cb` **再被调一次**。
- **面板必须容忍 `cb` 被调用两次**（文案幂等，不要做"停止后清空表单"之类的副作用）。

---

## 6. 自然结束与陈旧通知

- Timed / Counted 跑完 → 循环正常退出 → `reason = Completed` → 通知 → `state = Stopped`。
- 此时线程已死但 **`joinable()` 仍为 true**，所以「在跑」只能看 `state`，不能看 `joinable()`。
- 下一次 Start 会在 `launch` 步骤 2 回收它（join 立即返回）。
- Timed 用严格 `now < start + durationSec`，最后一轮可能被边界挡掉 → 实际轮数可能比 `时长 ÷ 间隔` 少 1。
- `clampWait` 只在 Timed 模式把等待截短到「距截止点的剩余时间」，保证「定时 10 秒 + 间隔 60 秒」真的 10 秒左右停。
- **序号只在 Start 推进**：Timed 任务结束的通知还在队列里、用户抢先开始新任务时，旧通知 `gen` 不匹配会被丢弃，不会把新任务刷成「已停止」。

---

## 7. 热键入口

`wxEVT_HOTKEY`（`MainFrame.cpp:65` 绑定）→ `MainFrame::onHotkey`（:123）→ `HotkeyManager::dispatch` →
`MainFrame::dispatchHotkeyAction` → 面板公开方法。

**与按钮走完全同一条链路**，不需要额外处理。注意「全部停止」会**串行**调多个 `StopXxx`，每个最多阻塞约 20ms。

---

## 8. 关窗

```
MainFrame::onClose → saveWindowState() + event.Skip()
  → ~MainFrame
      ├─ m_threadMgr.reset()      ← 先停线程（必须在 m_timer 释放之前）
      │     └─ ~ThreadManager → StopAll()          ThreadManager.cpp:280
      │           1) 所有槽 request_stop()（并行）
      │           2) 逐个 join() + state = Stopped
      └─ m_timer 释放
  → 未执行的 CallAfter 事件由 ~wxEvtHandler 的 DeletePendingEvents() 清掉
```

`StopAll` 先全叫停再逐个收，四个任务同时在跑也只等约 20ms（而不是 4 × 20ms）。

---

## 9. 计时器生命周期

- 唯一替换点是 `MainFrame::rebuildTimer()`（`MainFrame.cpp:195`）；
- 触发点：设置页切模式（`SettingsPanel.cpp:386`）、改 Hybrid 末尾忙等（`:410`）；
- **只影响此后启动的任务**：运行中的任务握着旧 `shared_ptr`，旧实例活到它结束；
- 干跑构建（`KBMACRO_DRY_RUN`）里 `SendInput` 被 `if constexpr` 编掉，但 `hold` 与 `interval` 的等待**照常发生**（时间保真）。

---

## 10. 接线 TODO 清单

- [ ] `MainFrame` 增 `std::unique_ptr<ThreadManager> m_threadMgr`（声明在 `m_timer` **之后**），ctor 创建，`~MainFrame` 先 `reset()`
- [ ] 三个面板构造签名加 `ThreadManager &`，保存引用
- [ ] `ClickerPanel.cpp:318` 的 `TODO` 换成 `StartClicker` + `FinishCallback`
- [ ] `KeyboardPanel.cpp` 的 start/stop 同理（另有 `sendOnce()` 走 UI 线程直调，需自己 `try/catch`）
- [ ] `RecorderPanel.cpp` 的录制/回放接 `StartRecord` / `StartPlayback`
- [ ] 面板运行时禁用按钮（现在 `refreshEnableStates` 只控 spin 框）；录制与回放互相禁用
- [ ] 状态栏三态文案（已完成 N 次 / 已停止 / 异常结束）+ 中英文资源
- [ ] `UpdateWindow` 或等价手段让「开始」后状态栏立即刷新（worker 很快时通知会先到）

---

## 11. 已知坑

1. **`cb` 可能被调两次**（Stop 后的回音）→ 文案幂等。
2. **`executed` 与配置次数不等**：被打断时更小，`Failed` 时为 0 → 文案别写死配置值。
3. **`Failed` 要显示「异常结束」**，不是「已停止」；异常文本已在 `launch` 的 catch 里记了日志。
4. **Stop 会冻结 UI** 约 20ms（Sleep）；「全部停止」串行调用会叠加 → 需要时改成先全叫停。
5. **录制/回放互斥由 `ThreadManager` 拒绝**（`StartRecord` 检查 Playback、`StartPlayback` 检查 Record），面板必须按 `IsRunning` 禁用对方按钮，否则用户点了没反应。
6. **`input::init()` 只在 `MainFrame` 构造里调一次**；干跑构建会多一条 `DRY RUN enabled` 的 WARN。
