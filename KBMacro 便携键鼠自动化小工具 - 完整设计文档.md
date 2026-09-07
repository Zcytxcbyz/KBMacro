# KBMacro-MVP 便携键鼠自动化小工具 - 完整设计文档

**版本：MVP-2.15**  
**许可：GPL‑v3**  
**平台：仅 Windows x64（Win7 / 10 / 11）**  
**开发环境：VS2026 + CMake + Conan 2 + Ninja + Python 3.8+**


## 更新说明

### MVP-2.15（当前版本）

**日志接口改用 `std::format`**

· 日志宏改为支持 `std::format` 格式化字符串，取代手动 `std::string` 拼接
· 使用 C++20 标准库，零额外依赖，编译时类型安全
· 日志写法从 `"pos=(" + std::to_string(x) + "," + std::to_string(y) + ")"` 变为 `"pos=({}, {})", x, y`
· `LOG_DEBUG_FREQ` 同步支持 `std::format` 格式化
· 所有日志调用处代码更简洁、更安全、更易读

### MVP-2.14

· 新增 `LOG_DEBUG_FREQ` 频率控制日志宏

### MVP-2.13

· 明确 Python 辅助工具定位

### MVP-2.12

· 新增 `src/common/ConfigFileHelper.hpp` 公共配置 IO 类
· 明确开发顺序：日志优先


## 1. 项目概述

KBMacro 是一款轻量便携的 Windows 键鼠自动化工具，输出单静态 EXE，无需安装，U 盘可直接运行。

**三大核心能力：**
- 鼠标连点
- 键盘模拟重复
- 键鼠录制与回放

**设计原则：**
- 个人开源小工具，非企业级
- 核心功能保证稳定
- 边缘异常做简化处理
- 使用启动静态多语言，不支持运行时热切换，修改语言需重启程序

**关键特性：**
- 后台独立线程执行自动化任务，UI 全程不阻塞
- 全局自定义热键，包含一键全局紧急停止
- 配置持久化保存；录制脚本导出/导入 `*.macro.json`
- 连点器/键盘模拟器参数导出/导入 `*.clicker.json` / `*.keyboard.json`
- 静态多语言（启动加载，需重启生效）
- 便携配置模式（同目录 `{exe}.json` 优先于 AppData）
- 所有许可证文本嵌入 EXE，单文件分发
- 依赖通过 Conan 2 管理，使用 Ninja 生成器构建


## 2. 项目边界（明确不做）

- ❌ 脚本语言、条件分支、变量、表达式
- ❌ 图像找图、找色、窗口识别
- ❌ 指定时刻定时启动
- ❌ 托盘后台、开机自启、联网更新
- ❌ 跨平台支持、脚本加密、付费功能
- ❌ 运行时动态切换语言
- ❌ 配置损坏 bak 备份文件
- ❌ 高精度抗漂移计时（sleep_until）


## 3. 功能需求

### 3.1 鼠标连点器（标签页1）

| 功能项 | 说明 |
|--------|------|
| 坐标设置 | X/Y 输入框，【捕捉坐标】按钮读取当前光标位置填入 |
| 忽略坐标 | 勾选后忽略 X/Y，直接点击当前光标位置；坐标输入框置灰 |
| 点击类型 | 左键单击、右键单击、左键双击、中键单击、滚轮上/下/左/右、侧键1（后退）/ 侧键2（前进） |
| 滚轮步数 | 1-10 格可调，默认 1 格（仅滚轮类型有效） |
| 点击间隔 | 10-60000ms 可调，默认 100ms（两次点击之间的等待） |
| 点击保持时间 | 鼠标按下到抬起之间的延时，5-200ms 可调，默认 20ms（滚轮无 holdMs） |
| 运行模式 | 下拉选择：无限循环 / 定时运行（秒） / 固定次数 |
| 次数输入 | 固定次数模式下启用，范围 1-999999，默认 1 |
| 控制 | 界面按钮启动/停止；支持自定义全局热键启停 |
| 参数导出 | 【导出参数】按钮，保存当前参数为 `*.clicker.json` |
| 参数加载 | 【加载参数】按钮，从 `*.clicker.json` 恢复参数 |
| 线程模型 | 独立后台线程运行；参数仅内存临时保存，不写入配置 |

**数据结构：**
```cpp
struct ClickerParams {
    int x, y;
    bool ignoreCoord;
    MouseButton button;
    RunConfig run;          // 统一运行参数
    int intervalMs;         // 点击间隔，默认 100ms，范围 10-60000ms
    int holdMs;             // 点击保持时间，默认 20ms，范围 5-200ms（滚轮无效）
    int wheelDelta;         // 滚轮步数，默认 1，范围 1-10（仅滚轮类型有效）
};
```

**鼠标按钮枚举：**
```cpp
enum class MouseButton {
    Left, Right, Middle, LeftDouble,
    WheelUp, WheelDown, WheelLeft, WheelRight,
    XButton1, XButton2
};
```

**`*.clicker.json` 格式：**
```json
{
    "x": 500,
    "y": 300,
    "ignore_coord": false,
    "button": "left",
    "interval_ms": 100,
    "hold_ms": 20,
    "wheel_delta": 1,
    "run": {
        "mode": "infinite",
        "duration_sec": 10,
        "repeat_count": 1
    }
}
```

### 3.2 键盘模拟器（标签页2）

| 功能项 | 说明 |
|--------|------|
| 按键设定 | 热键捕获对话框录入，支持 Ctrl/Shift/Alt/Win 修饰键组合，最多三键组合 |
| 操作 | 发送一次、开始重复发送、停止重复 |
| 重复间隔 | 可调 ms，默认 500ms（两次按键之间的等待） |
| 按键保持时间 | 按键按下到抬起之间的延时，5-200ms 可调，默认 50ms |
| 运行模式 | 下拉选择：无限循环 / 定时运行（秒） / 固定次数（仅重复发送模式有效） |
| 次数输入 | 固定次数模式下启用，范围 1-999999，默认 1 |
| 控制 | 支持自定义全局热键启停 |
| 参数导出 | 【导出参数】按钮，保存当前参数为 `*.keyboard.json` |
| 参数加载 | 【加载参数】按钮，从 `*.keyboard.json` 恢复参数 |
| 线程模型 | 独立后台线程 |

**数据结构：**
```cpp
struct KeyboardParams {
    std::vector<WORD> vkCodes;
    RunConfig run;          // 统一运行参数
    int intervalMs;         // 重复间隔，默认 500ms，范围 10-60000ms
    int holdMs;             // 按键保持时间，默认 50ms，范围 5-200ms
};
```

**`*.keyboard.json` 格式：**
```json
{
    "vk_codes": [0x41],
    "interval_ms": 500,
    "hold_ms": 50,
    "run": {
        "mode": "infinite",
        "duration_sec": 10,
        "repeat_count": 1
    }
}
```

### 3.3 任务录制、编排、回放（标签页3）

使用 Windows 低级钩子 `WH_KEYBOARD_LL` / `WH_MOUSE_LL`，钩子挂载于 UI 主线程消息循环。**停止录制、程序退出必须卸载钩子。**

#### 3.3.1 录制
- 捕获鼠标移动、各类点击、键盘按下/抬起事件
- 捕获滚轮事件（`WM_MOUSEWHEEL` / `WM_MOUSEHWHEEL`）和侧键事件（`XBUTTON1` / `XBUTTON2`）
- 自动记录当前屏幕分辨率（宽×高）和 DPI，存入每个 `Action`
- 自动计算动作之间时间差，转为 delayMs
- 鼠标/键盘按下-抬起配对：记录按下和抬起之间的时间差，存入 holdMs（滚轮无 holdMs）
- 动作序列读写使用 `MacroSequence` 容器（内置互斥锁保护）
- 回放时必须过滤 `LLMHF_INJECTED` 标志，避免录制到自身模拟的事件

#### 3.3.2 编排
- 列表可视化展示动作
- 支持新增动作、删除选中、上移、下移、修改单动作延时（delayMs）和保持时间（holdMs）
- 滚轮动作新增 wheelDelta 字段可编辑

#### 3.3.3 回放
- 顺序执行动作序列
- 每个点击/按键动作：按下 → 等待 holdMs → 抬起 → 等待 delayMs（滚轮直接执行滚动 → 等待 delayMs）
- 回放模式：下拉选择 无限循环 / 定时回放（秒） / 固定次数
- 固定次数：回放完整序列 N 次后自动停止
- 按钮/热键随时中止回放
- 回放时检测屏幕环境是否变化，弹窗询问用户如何适配坐标（支持会话内记住选择）

#### 3.3.4 脚本 IO
- 导出/加载 `*.macro.json` 动作脚本
- 脚本仅存储动作序列，不包含回放模式/次数（运行参数由 UI 临时指定）

**数据结构：**
```cpp
enum class ActionType {
    MouseMove,
    MouseLeftDown, MouseLeftUp, MouseLeftClick,
    MouseRightDown, MouseRightUp, MouseRightClick,
    MouseMiddleClick,
    WheelUp, WheelDown, WheelLeft, WheelRight,
    XButton1Click, XButton2Click,
    KeyDown, KeyUp, KeyPress
};

struct Action {
    ActionType type;
    POINT pos;
    WORD vkCode;
    DWORD scanCode;
    int delayMs;         // 动作结束后等待时间
    int holdMs;          // 按下→抬起的保持时间（滚轮无效）
    int wheelDelta;      // 滚轮步数，默认 1，范围 1-10（仅滚轮类型有效）
    int screenWidth;     // 录制时的屏幕宽度
    int screenHeight;    // 录制时的屏幕高度
    int dpi;             // 录制时的 DPI（如 96、144、192）
};

// 回放参数（UI 临时指定，不持久化）
struct PlaybackParams {
    RunConfig run;       // 统一运行参数
};
```

#### 3.3.5 屏幕适配流程

**录制时：**
```cpp
Action action;
action.screenWidth = GetSystemMetrics(SM_CXSCREEN);
action.screenHeight = GetSystemMetrics(SM_CYSCREEN);
action.dpi = GetDpiForWindow(mainFrame->GetHWND());
```

**回放时检测环境变化：**
```cpp
bool NeedAdaptation(const Action& action) {
    int currentW = GetSystemMetrics(SM_CXSCREEN);
    int currentH = GetSystemMetrics(SM_CYSCREEN);
    int currentDPI = GetDpiForWindow(mainFrame->GetHWND());
    return (action.screenWidth != currentW) ||
           (action.screenHeight != currentH) ||
           (action.dpi != currentDPI);
}
```

**环境变化时弹窗：**
```
┌──────────────────────────────────────────────────────────────────────┐
│ 屏幕环境变化                                                        │
│                                                                     │
│ 录制时屏幕: 1920 x 1080, DPI 96%                                  │
│ 当前屏幕:   2560 x 1440, DPI 150%                                 │
│                                                                     │
│ 坐标可能需要调整才能准确定位。是否自动转换坐标？                    │
│                                                                     │
│  [ 转换 ]  [ 使用原始坐标 ]  [ 取消回放 ]                          │
│  ☑ 本次会话记住此选择（不再询问）                                  │
└──────────────────────────────────────────────────────────────────────┘
```

| 选项 | 效果 |
|------|------|
| **转换** | 按比例缩放所有坐标 |
| **使用原始坐标** | 不转换，直接使用录制的坐标 |
| **取消回放** | 中止本次回放 |
| **记住选择** | 当前会话中，相同环境组合不再弹窗 |

**坐标转换逻辑：**
```cpp
if (user_chose_convert) {
    for (auto& action : sequence) {
        if (IsMouseAction(action.type)) {
            action.pos.x = action.pos.x * currentWidth / action.screenWidth;
            action.pos.y = action.pos.y * currentHeight / action.screenHeight;
        }
    }
    LOG_INFO("Macro", "坐标适配: {}x{}({}%) → {}x{}({}%)",
             action.screenWidth, action.screenHeight, action.dpi,
             currentWidth, currentHeight, currentDPI);
}
```

**记住选择机制：**
```cpp
struct AdaptDecision {
    ScreenEnvironment recorded;
    ScreenEnvironment current;
    bool adapt;
};
std::optional<AdaptDecision> m_rememberedAdapt;
```

### 3.4 设置（标签页4）

设置页是第四个标签页，集中管理程序的所有全局配置。

#### 3.4.1 语言设置
- 下拉选择框：当前支持 `zh-CN` / `en-US`，架构可扩展至更多语言
- 首次启动自动检测系统语言
- 修改后弹窗询问：立即重启 / 稍后手动重启
- 程序启动时根据配置加载对应语言资源

#### 3.4.2 配置管理（便携模式）
- 显示当前配置来源：`本地便携配置（EXE目录）` / `用户配置（AppData）`
- 【导出配置到 EXE 目录】按钮：将当前配置写入 `{exe文件名}.json`
- 【删除便携配置，恢复使用 AppData】按钮：删除同目录配置文件，回退到 AppData
- 配置加载优先级：
  1. 程序同目录下的 `{exe文件名}.json`（优先）
  2. 如果①不存在，则读取 `%APPDATA%\KBMacro\config.json`（回退）
  3. 如果①和②都不存在，则生成默认配置（写入 AppData）
- 当存在同目录配置时，所有配置修改默认写入同目录配置
- 当使用 AppData 配置时，所有配置修改写入 AppData

#### 3.4.3 热键配置
- 列表展示所有业务热键的当前绑定
- 每个热键带【修改】按钮，点击弹出热键捕获对话框
- 支持的热键列表：
  - 鼠标连点器：启动 / 停止
  - 键盘模拟器：启动重复 / 停止
  - 录制：开始录制 / 停止录制
  - 回放：运行序列 / 停止回放
  - 全局紧急停止（Ctrl+F12 默认）

#### 3.4.4 计时器模式
- 下拉或单选：`普通模式（省CPU，默认）` / `高精度模式（占用高）`
- 对应 `SleepTimerStrategy` / `SpinTimerStrategy`
- 保存到 config.json，程序启动时加载
- 切换到高精度模式时弹窗提示 CPU 占用升高，支持会话内记住选择

#### 3.4.5 窗口置顶
- 复选框：`窗口置顶`（`ID_CHK_ALWAYS_ON_TOP`）
- 勾选后主窗口置顶（`wxSTAY_ON_TOP`），取消勾选取消置顶
- 状态保存到 `config.json`（`always_on_top` 字段）
- 程序启动时恢复置顶状态
- 立即生效，无需重启

#### 3.4.6 记忆管理
- 【清除适配记忆】按钮：清除所有屏幕适配的“记住选择”
- 【清除高精度确认】按钮：清除高精度模式切换的“记住选择”
- 点击后对应的记忆状态重置，下次触发时重新弹窗

#### 3.4.7 恢复默认设置
- 【恢复默认设置】按钮（`ID_BTN_RESTORE_DEFAULTS`）
- 点击后弹出确认对话框：“确定要恢复所有设置到默认值吗？此操作不可撤销。”
- 恢复内容：
  - `language`：回退到首次启动自动检测
  - `window`：`{x:100, y:100, width:800, height:600, maximized:false}`
  - `selected_tab`：`0`
  - `timer_mode`：`"sleep"`
  - `always_on_top`：`false`
  - `hotkeys`：全部恢复默认热键
- 语言恢复后仍弹窗询问是否立即重启
- 操作记录到调试日志

### 3.5 关于与许可（标签页5）

第五个标签页展示程序信息和完整的许可证文本，所有文本均从 EXE 资源加载，不依赖外部文件。

#### 3.5.1 程序信息区域
- 程序名称（大号字体）
- 版本号
- 项目链接（wxHyperlinkCtrl）

#### 3.5.2 GPL-3.0 许可证
- 显示完整的 GPL-3.0 许可证原文
- 从资源 `MY_LICENSE`（RCDATA）加载
- 对应项目根目录的 `LICENSE.txt`

#### 3.5.3 第三方许可证

| 依赖库 | 许可证 | 说明 |
|--------|--------|------|
| wxWidgets | LGPL-2.1 | GUI 框架 |
| nlohmann-json | MIT | JSON 解析 |
| GTest | BSD-3-Clause | 单元测试（仅开发依赖） |

#### 3.5.4 UI 实现
- 使用 `wxTextCtrl` 设置为只读 + 多行 + 可复制
- 用户可滚动查看完整文本
- 所有许可证全文在同一个 `wxTextCtrl` 中显示，用分隔线区分


## 4. 全局公共能力

### 4.1 全局紧急停止热键
- 默认 Ctrl+F12，一键终止连点器、键盘重复、回放全部后台任务
- 设置原子停止标记，等待线程 join 回收

### 4.2 全局热键管理器
- Win32 API：`RegisterHotKey` / `UnregisterHotKey`
- 全部业务热键支持用户自定义
- 注册失败弹窗提示热键冲突，交由用户重新设置
- 程序关闭时统一注销全部已注册热键
- 热键配置持久化存入 config.json

### 4.3 配置持久化 config.json / {exe}.json

**配置加载优先级：**
1. 程序同目录下的 `{exe文件名}.json`（优先）
2. 如果①不存在，则读取 `%APPDATA%\KBMacro\config.json`（回退）
3. 如果①和②都不存在，则生成默认配置（写入 AppData）

**存储内容：**
- `language`：语言标识 zh-CN / en-US
- `window`：窗口位置、大小、最大化状态
- `selected_tab`：当前选中标签页索引
- `hotkeys`：全部自定义热键
- `timer_mode`：计时器模式（"sleep" / "spin"）
- `always_on_top`：窗口置顶（true / false）

**容错策略：** 文件损坏、解析异常，直接加载默认配置覆盖写入，不做备份 bak。

**格式示例：**
```json
{
  "language": "zh-CN",
  "timer_mode": "sleep",
  "always_on_top": false,
  "window": {
    "x": 100,
    "y": 100,
    "width": 800,
    "height": 600,
    "maximized": false
  },
  "selected_tab": 0,
  "hotkeys": {
    "clicker_start": { "mod": 2, "vk": 112 },
    "clicker_stop": { "mod": 2, "vk": 113 },
    "keyboard_start": { "mod": 2, "vk": 114 },
    "keyboard_stop": { "mod": 2, "vk": 115 },
    "record_start": { "mod": 2, "vk": 116 },
    "record_stop": { "mod": 2, "vk": 117 },
    "play_start": { "mod": 2, "vk": 118 },
    "play_stop": { "mod": 2, "vk": 119 },
    "emergency_stop": { "mod": 2, "vk": 123 }
  }
}
```

mod 字段取值：1=Alt，2=Ctrl，4=Shift，8=Win，可组合（如 3=Alt+Ctrl）。

### 4.4 参数文件格式

#### 4.4.1 `*.clicker.json`（鼠标连点器参数）
```json
{
    "x": 500,
    "y": 300,
    "ignore_coord": false,
    "button": "left",
    "interval_ms": 100,
    "hold_ms": 20,
    "wheel_delta": 1,
    "run": {
        "mode": "infinite",
        "duration_sec": 10,
        "repeat_count": 1
    }
}
```

#### 4.4.2 `*.keyboard.json`（键盘模拟器参数）
```json
{
    "vk_codes": [0x41],
    "interval_ms": 500,
    "hold_ms": 50,
    "run": {
        "mode": "infinite",
        "duration_sec": 10,
        "repeat_count": 1
    }
}
```

#### 4.4.3 `*.macro.json`（宏脚本）
```json
{
  "actions": [
    { "type": "mouse_move", "x": 500, "y": 300, "delay": 50 },
    { "type": "mouse_left_click", "x": 500, "y": 300, "delay": 100, "hold": 20 },
    { "type": "wheel_up", "x": 500, "y": 300, "delay": 50, "delta": 3 },
    { "type": "xbutton1_click", "x": 500, "y": 300, "delay": 50, "hold": 20 },
    { "type": "key_press", "vk": 0x41, "scan": 0x1E, "delay": 30, "hold": 50 }
  ],
  "screen_width": 1920,
  "screen_height": 1080,
  "dpi": 96
}
```

### 4.5 公共配置 IO 类（`ConfigFileHelper`）

为减少三个模块在参数导出/加载中的重复代码，统一封装文件对话框 + JSON 读写 + 异常处理。

**位置：** `src/common/ConfigFileHelper.hpp`

**接口设计（方案 A——纯静态工具类）：**
```cpp
class ConfigFileHelper {
public:
    static bool SaveToFile(const nlohmann::json& data,
                           wxWindow* parent,
                           const wxString& defaultExt,
                           const wxString& wildcard);

    static std::optional<nlohmann::json> LoadFromFile(wxWindow* parent,
                                                       const wxString& defaultExt,
                                                       const wxString& wildcard);
};
```

各模块只需实现自己的 `ToJson` / `FromJson` 函数，文件对话框和 IO 操作由 `ConfigFileHelper` 统一处理。

### 4.6 静态多语言模块（启动加载，需重启生效）
1. `res/resource.rc` 主文件通过 `#include` 引用各子资源文件
2. `resources/languages/` 目录存放语言字符串表，文件名与语言代号一一对应
3. 程序启动时：
   - 读取 config.json 的 `language` 字段
   - 若存在且有效，使用该语言
   - 若不存在，检测系统语言（`wxLocale::GetSystemLanguage()`）
   - 匹配不到或出现其他语言 → 默认英语
   - 保存检测结果到 config.json
4. 使用 `wxLocale` 一次性加载对应语言资源
5. 所有 UI 控件创建阶段直接调用 `wxGetTranslation(ID)` 获取文本
6. 设置页面提供语言下拉选择框；修改保存配置后弹窗提示用户选择：立即重启 / 稍后手动重启
7. 无运行时动态刷新控件 Label 逻辑

### 4.7 许可证嵌入（EXE 资源）
1. 项目根目录放置 `LICENSE.txt`（GPL-3.0 完整文本）
2. 项目根目录放置 `THIRD_PARTY_LICENSES.txt`（所有第三方依赖许可证汇总）
3. `resources/licenses.rc` 中使用 RCDATA 嵌入
4. 关于页通过 `FindResource` / `LoadResource` / `LockResource` 加载文本
5. 使用 `wxTextCtrl` 展示，只读、多行、支持滚动和复制

### 4.8 底层 API 封装

| 用途 | API |
|------|-----|
| 键鼠模拟 | `SendInput` |
| 录制钩子 | `WH_KEYBOARD_LL` / `WH_MOUSE_LL` |
| 全局热键 | `RegisterHotKey` / `UnregisterHotKey` |
| 光标捕捉 | `GetCursorPos` |
| 管理员检测 | `IsUserAnAdmin` |
| 获取 DPI | `GetDpiForWindow` / `SetProcessDPIAware` |

### 4.9 程序关闭资源清理（必须执行）
1. 设置全部后台线程原子停止标志，调用 join 等待线程退出
2. 如果正在录制，调用 `UnhookWindowsHookEx` 卸载钩子
3. 注销全部注册的全局热键
4. 保存 config.json 配置

### 4.10 提示与免责
1. UIPI 权限隔离：模拟高权限窗口需要管理员运行；启动检测后弹窗提示
2. 杀毒软件告警：低级钩子存在误报风险，README 中说明
3. 个人开源工具，按原样提供，风险由使用者自行承担


## 5. 交互确认框规范

### 5.1 四种对话框类型

| 对话框 | 触发场景 | 选项 | 记住选择 |
|--------|----------|------|----------|
| **屏幕适配** | 录制与回放屏幕环境不一致 | 转换 / 原始坐标 / 取消回放 | ✅ 会话内有效 |
| **高精度模式** | 用户切换到高精度模式 | 我知道了 | ✅ 会话内有效 |
| **语言切换** | 用户修改语言设置 | 立即重启 / 稍后手动重启 | ❌ 不适用 |
| **恢复默认设置** | 用户点击恢复默认设置按钮 | 确认 / 取消 | ❌ 不适用 |

### 5.2 会话定义
“会话”指程序从启动到关闭的完整运行周期。所有“记住选择”仅存在于内存中，程序关闭后销毁。

### 5.3 记住选择存储
```cpp
class MacroEngine {
    std::optional<AdaptDecision> m_rememberedAdapt;
    bool m_highPrecisionConfirmed = false;
};
```

### 5.4 手动重置记忆

| 控件 ID | 默认中文文本 | 用途 |
|---------|-------------|------|
| ID_BTN_CLEAR_ADAPT_MEMORY | 清除适配记忆 | 清除屏幕适配的“记住选择” |
| ID_BTN_CLEAR_HIGH_PRECISION_MEMORY | 清除高精度确认 | 清除高精度模式的“记住选择” |


## 6. 调试日志模块

### 6.1 概述

`DebugLog` 是专门为开发调试设计的日志模块，仅 Debug 模式下启用，Release 模式完全移除。

**核心设计原则：**
- 条件编译隔离（`#ifdef _DEBUG`）
- 每次启动生成独立日志文件
- 分模块标签便于定位问题
- 四级日志级别（ERROR / WARN / INFO / DEBUG）
- 实时写入确保崩溃时日志不丢失
- 频率控制：高频场景下按比例记录日志，避免文件膨胀
- `std::format` 格式化：简洁、类型安全、零额外依赖

**开发顺序：日志优先**

调试日志模块是**整个项目第一个需要开发和测试的模块**。

**日志文件位置：**
```
KBMacro/logs/
├─ debug_2026-09-07_14-23-45.log
├─ debug_2026-09-07_16-10-12.log
└─ debug_2026-09-08_09-05-33.log
```

文件命名格式：`debug_YYYY-MM-DD_HH-MM-SS.log`，每次启动自动创建新文件。

### 6.2 日志级别定义

| 级别 | 标识 | 含义 | 使用场景 |
|------|------|------|----------|
| **ERROR** | `[ERR]` | 发生了错误，功能可能受影响 | 配置加载失败、钩子安装失败、热键注册冲突 |
| **WARN** | `[WRN]` | 发生了意外但程序能继续运行 | JSON 字段缺失、配置文件损坏回退 |
| **INFO** | `[INF]` | 正常的操作记录 | 程序启动、线程启停、配置加载成功 |
| **DEBUG** | `[DBG]` | 详细的调试信息 | 每次点击、事件入队、坐标适配、参数导出/加载 |

### 6.3 模块标签定义

| 标签 | 对应模块 | 记录内容 |
|------|----------|----------|
| `[Main]` | 主窗口/程序入口 | 程序启动、窗口创建、资源清理顺序 |
| `[Config]` | 配置管理器 | 配置加载/保存、路径、解析错误 |
| `[Hotkey]` | 热键管理器 | 热键注册/注销、冲突检测、热键触发 |
| `[Clicker]` | 鼠标连点器 | 线程启停、每次点击、参数导出/加载 |
| `[Keyboard]` | 键盘模拟器 | 线程启停、按键发送、参数导出/加载 |
| `[Macro]` | 录制回放引擎 | 钩子安装/卸载、事件入队、配对结果、坐标适配 |
| `[Settings]` | 设置页面 | 语言切换、热键修改、置顶切换、记忆清除、恢复默认设置 |
| `[Timer]` | 计时器工具 | 策略切换、等待开始/完成/被中断 |

### 6.4 日志格式
```
2026-09-07 14:23:45.123 [Macro] [TID:1234] [INF] WH_MOUSE_LL installed, result=SUCCESS
```

### 6.5 日志类接口

```cpp
// DebugLog.hpp
#ifdef _DEBUG

#include <format>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <thread>
#include <string>
#include <unordered_map>

class DebugLog {
public:
    static DebugLog& GetInstance();

    // 核心写入方法（接受 std::format 风格的格式化字符串）
    template<typename... Args>
    void Write(const char* module, const char* level, std::format_string<Args...> fmt, Args&&... args) {
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        WriteImpl(module, level, msg);
    }

    // 频率控制版本
    template<typename... Args>
    void WriteFreq(const char* module, const char* level,
                   std::format_string<Args...> fmt, size_t frequency, Args&&... args) {
        if (frequency <= 0) return;
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        WriteFreqImpl(module, level, msg, frequency);
    }

    // 重置频率计数器
    void ResetFreqCounter(const std::string& key);

private:
    void WriteImpl(const char* module, const char* level, const std::string& msg);
    void WriteFreqImpl(const char* module, const char* level, const std::string& msg, size_t frequency);
    std::string GetTimestamp();
    std::ofstream file_;
    std::mutex mtx_;

    struct FreqCounter {
        size_t count{0};
        size_t freq{1};
    };
    std::unordered_map<std::string, FreqCounter> m_freqCounters;
};

// 普通日志宏
#define LOG_ERROR(module, fmt, ...) \
    DebugLog::GetInstance().Write(module, "ERR", fmt, ##__VA_ARGS__)
#define LOG_WARN(module, fmt, ...) \
    DebugLog::GetInstance().Write(module, "WRN", fmt, ##__VA_ARGS__)
#define LOG_INFO(module, fmt, ...) \
    DebugLog::GetInstance().Write(module, "INF", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(module, fmt, ...) \
    DebugLog::GetInstance().Write(module, "DBG", fmt, ##__VA_ARGS__)

// 频率控制日志宏
#define LOG_DEBUG_FREQ(module, freq, fmt, ...) \
    DebugLog::GetInstance().WriteFreq(module, "DBG", fmt, freq, ##__VA_ARGS__)

#else
// Release 版本全部移除
#define LOG_ERROR(module, fmt, ...) ((void)0)
#define LOG_WARN(module, fmt, ...)  ((void)0)
#define LOG_INFO(module, fmt, ...)  ((void)0)
#define LOG_DEBUG(module, fmt, ...) ((void)0)
#define LOG_DEBUG_FREQ(module, freq, fmt, ...) ((void)0)
#endif
```

### 6.6 使用示例

**普通日志：**
```cpp
LOG_INFO("Main", "App started, version={}", version);
LOG_DEBUG("Macro", "EVENT: WM_LBUTTONDOWN, pos=({}, {}), flags=0x{:X}", x, y, flags);
LOG_DEBUG("Macro", "PAIRED: down={}, up={}, holdMs={}", downTime, upTime, holdMs);
LOG_ERROR("Hotkey", "Register failed: conflict with existing hotkey, mod={}, vk={}", mod, vk);
```

**频率控制日志：**
```cpp
// 每 1000 次记录一次
LOG_DEBUG_FREQ("Clicker", 1000, "Click sent, count={}", clickCount);
```

**日志输出示例：**
```
2026-09-07 14:23:45.123 [Main] [TID:1234] [INF] App started, version=v0.1.0-alpha
2026-09-07 14:23:45.456 [Macro] [TID:1234] [DBG] EVENT: WM_LBUTTONDOWN, pos=(340, 520), flags=0x0000
2026-09-07 14:23:45.812 [Macro] [TID:1234] [DBG] PAIRED: down=14:23:45.789, up=14:23:45.812, holdMs=23
2026-09-07 14:23:46.102 [Clicker] [TID:5678] [DBG] Click sent, count=1000
2026-09-07 14:23:46.203 [Clicker] [TID:5678] [DBG] Click sent, count=2000
```

### 6.7 频率控制重置

在新任务开始时重置计数器：
```cpp
DebugLog::GetInstance().ResetFreqCounter("Clicker|Click sent");
```

### 6.8 调试工具链分工

| 调试方法 | 适用场景 | 优势 |
|----------|----------|------|
| **断点调试** | 变量状态、调用栈、单步追踪 | 精确查看运行时数据 |
| **日志调试** | 钩子回调、多线程时序、长时间运行分析 | 非侵入式，不阻塞执行 |
| **单元测试** | 模块功能验证、回归测试 | 自动化，可重复执行 |


## 7. Python 辅助工具（后续阶段）

### 7.1 定位

Python 在整个项目中定位为**辅助工具**，不参与最终 EXE，仅在开发阶段使用。

```
Python 做：      C++ 做：
  自动化构建        主程序逻辑
  日志可视化        鼠标连点
  测试辅助          键盘模拟
  代码生成辅助      录制回放
  发布辅助          钩子/热键
  维护辅助          配置管理
```

**Python 是“帮你开发 C++ 项目的工具”，C++ 是“交付给用户的工具”。**

### 7.2 辅助工具清单

| 工具 | 用途 | 优先级 |
|------|------|--------|
| `tools/log_viewer/` | 日志可视化工具 | 后续阶段 |
| `tools/scripts/build.py` | 一键构建（Debug/Release） | 后续 |
| `tools/scripts/run_tests.py` | 运行单元测试 + 生成 HTML 报告 | 后续 |
| `tools/scripts/release.py` | 自动打包 + 生成 changelog + 上传 GitHub Release | 后续 |
| `tools/scripts/clean_logs.py` | 清理 7 天前的过期日志 | 后续 |
| `tools/codegen/gen_resource_ids.py` | 自动生成 `resource.h` 中 ID 定义 | 后续 |
| `tools/codegen/gen_language_template.py` | 从模板生成新语言 `.rc` 文件 | 后续 |

### 7.3 日志可视化工具（规划）

**工具定位：** 独立辅助工具，不在 MVP 范围内，后续阶段单独开发。

**功能需求：**
- 列出 `logs/` 目录下所有日志文件（按时间排序）
- 按模块标签筛选（`[Macro]`、`[Clicker]`、`[Hotkey]`...）
- 按日志级别筛选（ERROR / WARN / INFO / DEBUG）
- 按时间范围筛选
- 关键词搜索（支持正则表达式）
- 日志高亮显示（不同级别不同颜色）

**技术选型：**
- Python 3.8+
- tkinter 或 PyQt5（界面库）
- 正则表达式（内置）

### 7.4 目录结构
```
KBMacro/
├─ ... (现有 C++ 结构)
├─ tools/                        # Python 辅助工具目录
│   ├─ log_viewer/                # 日志可视化工具
│   │   ├─ log_viewer.py
│   │   ├─ requirements.txt
│   │   └─ README.md
│   ├─ scripts/                   # 构建/测试/发布脚本
│   │   ├─ build.py
│   │   ├─ run_tests.py
│   │   ├─ release.py
│   │   └─ clean_logs.py
│   └─ codegen/                   # 代码生成辅助
│       ├─ gen_resource_ids.py
│       └─ gen_language_template.py
└─ docs/                          # 设计文档
```


## 8. 架构与线程模型

### 8.1 模块划分

| 模块 | 职责 | 实现方式 |
|------|------|----------|
| UI 模块 | 主窗口、wxNotebook **五个标签页**、对话框、Sizer 布局 | wxWidgets |
| **公共组件层** | 后台线程基类、计时器策略、输入模拟、动作序列容器、运行参数、调试日志、配置 IO 辅助 | `src/common/` |
| 连点器模块 | 后台线程循环模拟鼠标点击（含滚轮/侧键） | 继承 `WorkerThread` + `InputSimulator` |
| 键盘模拟器模块 | 后台线程循环/单次模拟键盘 | 继承 `WorkerThread` + `InputSimulator` |
| 录制回放引擎 | 钩子回调、动作序列管理、回放执行 | `SetWindowsHookEx` + `MacroSequence` + `WorkerThread` |
| 热键管理器 | 注册/注销、回调绑定、配置读写（单例） | `RegisterHotKey` + nlohmann-json |
| 配置管理器 | 配置文件读写、容错处理、便携模式管理（单例） | nlohmann-json |
| 多语言管理器 | 启动时加载语言资源（单例） | `wxLocale` + RC 资源 |
| Win32 底层封装 | 系统 API 薄封装（命名空间） | 自由函数 |

### 8.2 概念架构简图
```
wxWidgets UI 主窗口
├─ 标签页1：鼠标连点器（含参数导出/加载）
├─ 标签页2：键盘模拟器（含参数导出/加载）
├─ 标签页3：录制编排回放（含屏幕适配）
├─ 标签页4：设置（含恢复默认设置）
└─ 标签页5：关于与许可
    │
    ▼
连点器模块 ｜ 键盘模块 ｜ 录制回放引擎
    │                              │
    └──────────┬───────────────────┘
               ▼
        公共组件层
    ├─ WorkerThread
    ├─ ITimerStrategy
    ├─ InputSimulator
    ├─ MacroSequence
    ├─ RunConfig
    ├─ DebugLog       ← 第一个开发（含频率控制 + std::format）
    └─ ConfigFileHelper
               │
               ▼
热键管理器 ｜ 配置管理器 ｜ 多语言管理器
               │
               ▼
Win32 API 封装层
```

### 8.3 线程规则

| 规则 | 说明 |
|------|------|
| UI 主线程 | 全部 wxWidgets 控件操作；低级钩子回调运行在 UI 主线程 |
| 后台工作线程 | 连点线程、键盘重复线程、回放线程，均继承 `WorkerThread` 基类 |
| UI 更新 | 后台线程禁止直接操作 UI 控件；使用 `CallAfter` / `wxQueueEvent` |
| 动作序列保护 | 容器读写使用 `MacroSequence` 封装（内置 `std::mutex`） |
| 线程终止 | 使用 `WorkerThread` 的原子停止标记 + `join` 等待退出 |

### 8.4 录制钩子关键实现
- 钩子挂载于 UI 主线程消息循环
- 回调函数中仅做数据封装（构造 Action）+ 快速入队（`MacroSequence::Append`）
- 按下-抬起配对：维护"按下事件暂存表"，计算时间差作为 holdMs
- 滚轮事件：直接记录 wheelDelta，无 holdMs
- 绝不做耗时操作
- 停止录制时直接调用 `UnhookWindowsHookEx` 卸载
- 回放时必须过滤 `LLMHF_INJECTED` 标志


## 9. 项目目录结构

```
KBMacro/
├─ cmake/
├─ profiles/
│   ├─ debug
│   └─ release
├─ res/
│   ├─ resource.rc
│   ├─ resource.h
│   └─ resources/
│       ├─ icons.rc
│       ├─ licenses.rc
│       └─ languages/
│           ├─ zh-CN.rc
│           ├─ en-US.rc
│           ├─ ja-JP.rc
│           ├─ ko-KR.rc
│           ├─ fr-FR.rc
│           └─ de-DE.rc
├─ src/
│   ├─ main.cpp
│   ├─ common/
│   │   ├─ Enums.hpp
│   │   ├─ Types.hpp
│   │   ├─ Constants.hpp
│   │   ├─ RunConfig.hpp
│   │   ├─ WorkerThread.hpp
│   │   ├─ MacroSequence.hpp
│   │   ├─ InputSimulator.hpp/cpp
│   │   ├─ Utils.hpp/cpp
│   │   ├─ DebugLog.hpp          ← 第一个开发（含频率控制 + std::format）
│   │   ├─ ConfigFileHelper.hpp
│   │   └─ timer/
│   │       ├─ ITimerStrategy.hpp
│   │       ├─ SleepTimerStrategy.hpp/cpp
│   │       ├─ SpinTimerStrategy.hpp/cpp
│   │       └─ TimerUtils.hpp/cpp
│   ├─ ui/
│   │   ├─ MainFrame.hpp/cpp
│   │   ├─ ClickerPanel.hpp/cpp
│   │   ├─ KeyboardPanel.hpp/cpp
│   │   ├─ MacroPanel.hpp/cpp
│   │   ├─ SettingsPanel.hpp/cpp
│   │   └─ AboutPanel.hpp/cpp
│   ├─ modules/
│   │   ├─ ClickerModule.hpp/cpp
│   │   ├─ KeyboardModule.hpp/cpp
│   │   └─ MacroEngine.hpp/cpp
│   ├─ managers/
│   │   ├─ HotkeyManager.hpp/cpp
│   │   ├─ ConfigManager.hpp/cpp
│   │   └─ LanguageManager.hpp/cpp
│   └─ win32wrap/
│       └─ Win32API.hpp/cpp
├─ tests/
│   ├─ CMakeLists.txt
│   ├─ test_clicker.cpp
│   ├─ test_keyboard.cpp
│   └─ test_macro_serialization.cpp
├─ tools/                         # Python 辅助工具目录
│   ├─ log_viewer/                 # 日志可视化工具
│   │   ├─ log_viewer.py
│   │   ├─ requirements.txt
│   │   └─ README.md
│   ├─ scripts/                    # 构建/测试/发布脚本
│   │   ├─ build.py
│   │   ├─ run_tests.py
│   │   ├─ release.py
│   │   └─ clean_logs.py
│   └─ codegen/                    # 代码生成辅助
│       ├─ gen_resource_ids.py
│       └─ gen_language_template.py
├─ LICENSE.txt
├─ THIRD_PARTY_LICENSES.txt
├─ conanfile.txt
├─ .gitignore
├─ .editorconfig
├─ .gitattributes
├─ CMakeLists.txt
├─ CMakePresets.json
└─ README.md
```


## 10. 构建流程

### 10.1 Conan 2 Profile 文件

**profiles/debug：**

```ini
[settings]
arch=x86_64
build_type=Debug
compiler=msvc
compiler.cppstd=20
compiler.runtime=static
compiler.runtime_type=Debug
compiler.version=195
os=Windows
[conf]
tools.build:jobs=8
tools.cmake.cmaketoolchain:generator="Ninja"
```

**profiles/release：**

```ini
[settings]
arch=x86_64
build_type=Release
compiler=msvc
compiler.cppstd=20
compiler.runtime=static
compiler.runtime_type=Release
compiler.version=195
os=Windows
[conf]
tools.build:jobs=8
tools.cmake.cmaketoolchain:generator="Ninja"
```

### 10.2 依赖拉取（Conan 2）
```bash
conan install . --output-folder=build/debug --build=missing --profile=profiles/debug
conan install . --output-folder=build/release --build=missing --profile=profiles/release
```

### 10.3 Conan 2 配置文件（conanfile.txt）
```txt
[requires]
wxwidgets/3.2.11
nlohmann_json/3.12.0
gtest/1.18.0

[generators]
CMakeToolchain
CMakeDeps

[options]
wxwidgets/*:shared=False
wxwidgets/*:aui=False
wxwidgets/*:opengl=False
wxwidgets/*:webview=False
wxwidgets/*:html=False
wxwidgets/*:mediactrl=False
wxwidgets/*:richtext=False
wxwidgets/*:help=False
wxwidgets/*:html_help=False

[layout]
cmake_layout
```

### 10.4 CMake 构建

1. 通过 cmake -B 直接构建
```bash
cmake -B build/debug -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=build/debug/conan_toolchain.cmake
cmake --build build/debug

cmake -B build/release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=build/release/conan_toolchain.cmake
cmake --build build/release
```
2. 通过 cmake --preset 预设构建 (推荐)
```bash
cmake --preset x64-debug

cmake --preset x64-release
```


### 10.5 编译选项
| 选项 | 设置 |
|------|------|
| 标准 | C++20 |
| 运行时库 | `/MT`（静态链接） |
| 优化 | `/O2`（Release） |
| 调试日志 | Debug 模式启用，Release 条件编译移除 |
| 输出 | 单 EXE 文件 |


## 11. 开发顺序

### 11.1 模块优先级

| 优先级 | 模块 | 原因 |
|--------|------|------|
| **P0（第一个）** | `DebugLog`（含频率控制 + `std::format`） | 所有后续模块的调试都依赖它 |
| **P0** | `DebugLog` 单元测试 | 确保日志系统本身稳定可靠 |
| **P1** | `ConfigFileHelper` | 三个模块的导出/加载都依赖它 |
| **P1** | `WorkerThread` | 三个业务模块的线程管理基类 |
| **P1** | `RunConfig` / `Enums` / `Types` | 公共数据结构 |
| **P2** | 三大业务模块 | 依赖上述基础设施 |

### 11.2 依赖关系图
```
DebugLog（日志）
    │
    ├── DebugLog 单元测试（验证日志本身）
    │
    ├── ConfigFileHelper（依赖 nlohmann-json + wxWidgets 文件对话框）
    │       │
    │       ├── ClickerPanel（参数导出/加载）
    │       ├── KeyboardPanel（参数导出/加载）
    │       └── MacroPanel（脚本导出/加载）
    │
    ├── WorkerThread（线程管理）
    │       │
    │       ├── ClickerModule
    │       ├── KeyboardModule
    │       └── MacroEngine
    │
    └── RunConfig / Enums / Types（公共数据结构）
            │
            └── 三大业务模块
```

### 11.3 MVP 阶段开发节奏
1. **第 1 步**：实现 `DebugLog`（含频率控制 + `std::format`）+ 单元测试
2. **第 2 步**：实现公共数据结构（`Enums.hpp`、`Types.hpp`、`Constants.hpp`、`RunConfig.hpp`）
3. **第 3 步**：实现 `ConfigFileHelper`
4. **第 4 步**：实现 `WorkerThread`
5. **第 5 步**：实现三大业务模块 + UI 面板


## 12. MVP 取舍清单

### ✅ 保留
- 三大完整核心功能
- 三种运行模式
- 后台线程安全启停
- 钩子正确安装卸载 + `LLMHF_INJECTED` 过滤
- 全局热键 + 一键紧急停止
- config.json 配置
- 便携配置模式（`{exe}.json`）
- 窗口位置大小记忆
- 窗口置顶功能
- 鼠标滚轮（4 方向）+ 侧键（2 个）
- 滚轮步数控制（1-10 格）
- 屏幕分辨率与 DPI 适配
- 会话内记住选择
- 记忆清除按钮
- 恢复默认设置按钮
- 连点器参数导出/加载（`*.clicker.json`）
- 键盘模拟器参数导出/加载（`*.keyboard.json`）
- 录制脚本导出/加载（`*.macro.json`）
- 静态多语言 + 首次启动自动检测
- 许可证嵌入 EXE
- 调试日志（条件编译）— **第一个开发**（含频率控制 + `std::format`）
- 公共配置 IO 类（`ConfigFileHelper`）

### ❌ 砍掉 / 简化
- 运行时动态切换语言
- 配置损坏 bak 备份文件
- 严格 JSON 校验
- sleep_until 高精度抗漂移计时
- 热键批量注册失败精细回滚
- 动作序列数量硬上限保护
- 屏幕适配永久记住
- Python 辅助工具（后续阶段）


## 13. 错误处理策略

| 场景 | 处理方式 |
|------|----------|
| 热键注册冲突 | 弹窗提示，用户重新设置 |
| 钩子安装失败 | 弹窗提示，建议管理员运行 |
| 配置文件损坏 | 静默覆盖为默认配置 |
| 便携配置加载失败 | 静默回退到 AppData 配置 |
| 脚本 JSON 解析失败 | 清空动作列表，弹窗提示 |
| 参数 JSON 解析失败 | 弹窗提示，不清空当前参数 |
| 许可证资源加载失败 | 显示"许可证不可用" |
| 线程异常 | 捕获 `std::exception`，UI 显示错误框 |
| 非管理员运行 | 启动时检测，弹窗提示 |


## 14. 性能考虑

- UI 响应：所有耗时操作均在后台线程，UI 不卡顿
- 钩子开销：仅在录制期间启用，停止后立即卸载
- 内存占用：动作序列建议不超过 10000 个动作（约 < 10MB）
- 计时精度：默认 Sleep 策略存在微小漂移；Spin 策略提升精度（CPU 占用升高）
- 调试日志：仅在 Debug 模式存在，Release 条件编译移除，零性能影响
- `std::format`：编译时类型检查，运行时高效，零额外依赖


## 15. 安全性

| 项目 | 说明 |
|------|------|
| UIPI 权限 | 启动时检测管理员身份 |
| 杀毒软件 | 低级钩子可能被误判，README 中说明 |
| 热键冲突 | 注册失败直接反馈 |
| 配置隐私 | 仅存储热键和窗口状态 |


## 16. 项目评估

| 指标 | 数值 |
|------|------|
| C++ 源码行数 | 2900-3600 行 |
| 开发周期 | 20-35 天（业余开发） |
| Release EXE 体积 | 1.5-2.5 MB（UPX 压缩后） |
| 第一个开发的模块 | `DebugLog`（含频率控制 + `std::format`） |


## 17. README.md 声明

```markdown
# KBMacro

GPL‑v3 开源个人键鼠自动化工具。

## 依赖要求

- Windows 10/11 (x64) 或 Windows 7 (x64)
- Visual Studio 2026 或更高版本（MSVC 195）
- Conan 2.0 或更高版本
- CMake 3.25 或更高版本
- Ninja

## 构建步骤

conan install . --output-folder=build/debug --build=missing --profile=profiles/debug
cmake --preset x64-debug
cmake --build build/debug

## 注意事项

1. 程序使用 Windows 低级钩子，杀毒软件可能产生误报。
2. 模拟输入无法作用于高权限窗口，需要请以管理员身份运行。
3. 语言修改后需要重启程序才会生效。
4. 配置文件损坏会直接重置为默认配置。
5. 程序支持便携模式：将 `{exe文件名}.json` 放在 EXE 同目录下，配置将跟随 EXE 移动。
6. 许可证文本已嵌入 EXE，可在"关于"页查看完整的 GPL-3.0 和第三方许可证。
7. 录制宏时屏幕环境（分辨率+DPI）会被记录，在不同屏幕回放时会弹窗询问是否适配坐标。
8. 连点器和键盘模拟器的参数可通过 `*.clicker.json` / `*.keyboard.json` 导出和加载。
```


## 18. 附录：UI 控件关键标识

### 鼠标连点器（标签页1）

| 控件 ID | 默认中文文本 | 用途 |
|---------|-------------|------|
| ID_BTN_CAPTURE | 捕捉坐标 | 读取当前光标位置 |
| ID_CHK_IGNORE_COORD | 忽略坐标 | 忽略 X/Y |
| ID_SPIN_X | X 坐标 | 点击 X 坐标 |
| ID_SPIN_Y | Y 坐标 | 点击 Y 坐标 |
| ID_COMBO_BUTTON | 点击类型 | 左键/右键/中键/双击/滚轮/侧键 |
| ID_SPIN_WHEEL_DELTA | 滚轮步数 | 1-10 格 |
| ID_SPIN_INTERVAL | 点击间隔(ms) | 两次点击之间的等待 |
| ID_SPIN_HOLD | 保持时间(ms) | 按下→抬起的脉冲宽度 |
| ID_COMBO_CLICK_MODE | 运行模式 | 无限/定时/固定次数 |
| ID_SPIN_CLICK_COUNT | 重复次数 | 固定次数模式专用 |
| ID_BTN_START_CLICK | 启动连点 | 启动后台线程 |
| ID_BTN_STOP_CLICK | 停止连点 | 停止后台线程 |
| ID_BTN_EXPORT_CLICKER | 导出参数 | 保存当前参数为 `*.clicker.json` |
| ID_BTN_IMPORT_CLICKER | 加载参数 | 从 `*.clicker.json` 恢复参数 |

### 键盘模拟器（标签页2）

| 控件 ID | 默认中文文本 | 用途 |
|---------|-------------|------|
| ID_BTN_SET_KEY | 设定按键 | 弹出热键捕获对话框 |
| ID_STATIC_KEY | （当前按键显示） | 显示已设定的按键组合 |
| ID_BTN_SEND_ONCE | 发送一次 | 单次发送按键 |
| ID_SPIN_KEY_INTERVAL | 重复间隔(ms) | 两次按键之间的等待 |
| ID_SPIN_KEY_HOLD | 按键保持(ms) | 按下→抬起的脉冲宽度 |
| ID_COMBO_KEY_MODE | 运行模式 | 无限/定时/固定次数 |
| ID_SPIN_KEY_COUNT | 重复次数 | 固定次数模式专用 |
| ID_BTN_START_REPEAT | 开始重复发送 | 启动后台线程 |
| ID_BTN_STOP_REPEAT | 停止 | 停止后台线程 |
| ID_BTN_EXPORT_KEYBOARD | 导出参数 | 保存当前参数为 `*.keyboard.json` |
| ID_BTN_IMPORT_KEYBOARD | 加载参数 | 从 `*.keyboard.json` 恢复参数 |

### 录制与编排（标签页3）

| 控件 ID | 默认中文文本 | 用途 |
|---------|-------------|------|
| ID_BTN_RECORD_START | 开始录制 | 安装钩子开始录制 |
| ID_BTN_RECORD_STOP | 停止录制 | 卸载钩子停止录制 |
| ID_LIST_ACTIONS | （动作列表） | wxListCtrl 展示动作序列 |
| ID_BTN_ADD_ACTION | 新增动作 | 插入空白动作 |
| ID_BTN_DEL_ACTION | 删除选中 | 删除选中的动作 |
| ID_BTN_MOVE_UP | 上移 | 上移选中动作 |
| ID_BTN_MOVE_DOWN | 下移 | 下移选中动作 |
| ID_SPIN_DELAY | 延时(ms) | 修改选中动作的 delay |
| ID_SPIN_HOLD_EDIT | 保持(ms) | 修改选中动作的 hold |
| ID_SPIN_WHEEL_EDIT | 滚轮步数 | 修改选中滚轮动作的 delta |
| ID_COMBO_PLAY_MODE | 回放模式 | 无限/定时/固定次数 |
| ID_SPIN_PLAY_COUNT | 回放次数 | 固定次数模式专用 |
| ID_BTN_PLAY | 运行序列 | 启动回放 |
| ID_BTN_PLAY_STOP | 停止回放 | 停止回放 |
| ID_BTN_SAVE_MACRO | 保存脚本 | 导出为 `*.macro.json` |
| ID_BTN_LOAD_MACRO | 加载脚本 | 导入 `*.macro.json` |

### 设置（标签页4）

| 控件 ID | 默认中文文本 | 用途 |
|---------|-------------|------|
| ID_COMBO_LANGUAGE | 语言 | zh-CN / en-US 下拉选择 |
| ID_STATIC_CONFIG_SOURCE | 配置来源 | 显示当前配置来源 |
| ID_BTN_EXPORT_CONFIG | 导出配置到 EXE 目录 | 导出便携配置 |
| ID_BTN_REMOVE_LOCAL_CONFIG | 删除便携配置，恢复使用 AppData | 切换回 AppData |
| ID_COMBO_TIMER_MODE | 计时器模式 | 普通模式 / 高精度模式 |
| ID_CHK_ALWAYS_ON_TOP | 窗口置顶 | 勾选后主窗口置顶 |
| ID_BTN_CLEAR_ADAPT_MEMORY | 清除适配记忆 | 清除屏幕适配的“记住选择” |
| ID_BTN_CLEAR_HIGH_PRECISION_MEMORY | 清除高精度确认 | 清除高精度模式的“记住选择” |
| ID_BTN_RESTORE_DEFAULTS | 恢复默认设置 | 一键恢复所有配置到默认值 |
| ID_BTN_MODIFY_HOTKEY_* | 修改 | 每个热键对应的修改按钮 |

### 关于与许可（标签页5）

| 控件 ID | 默认中文文本 | 用途 |
|---------|-------------|------|
| ID_STATIC_ABOUT_TITLE | KBMacro | 程序名称 |
| ID_STATIC_ABOUT_VERSION | 版本 1.0.0 | 版本号显示 |
| ID_HYPERLINK_PROJECT | GitHub 项目地址 | 项目链接 |
| ID_TEXT_LICENSES | 许可证 | 显示所有许可证全文 |


**文档版本：MVP-2.15**