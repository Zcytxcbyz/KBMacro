#pragma once

#include <array>
#include <string>
#include <nlohmann/json.hpp>

#include "common/HotkeyAction.hpp"
#include "common/Types.hpp"

// ============================================================
// 全局配置数据
//
// 只描述需要持久化的字段（全局设置：语言、计时器、窗口置顶、热键、标志位）。
//
// 不在这里：
//   - 各模块参数（ClickerParams / KeyboardParams / 录制脚本）
//     → 由各模块的"导入 / 导出参数"按钮独立持久化
//   - 运行时状态（线程、timer 实例、UI 控件值）
// ============================================================

// 配置 JSON 的 schema 版本。结构不兼容时 +1。
// 加载时若文件 version > kConfigVersion，说明文件来自更新版本，应拒绝加载。
inline constexpr int kConfigVersion = 1;

// 热键启用标志的默认值：全 true。
// 老配置文件没有 hotkeyEnabled 字段时走这个默认值，
// 保证升级后已配的热键不会被静默禁用。
inline constexpr auto kHotkeyEnabledDefault = [] {
    std::array<bool, kHotkeyCount> a{};
    a.fill(true);
    return a;
}();

// 默认热键：单键 F1–F11，按 HotkeyAction 的顺序一一对应
//   F1 开始连点 / F2 停止连点 / F3 开始键盘 / F4 停止键盘 / F5 单次发送
//   F6 开始录制 / F7 停止录制 / F8 开始回放 / F9 停止回放
//   F10 全部开始 / F11 停止全部
//
// vk 直接写数值，因为 Config.hpp 不包含 windows.h（VK_F1 == 0x70 … VK_F11 == 0x7A）。
inline constexpr std::array<Hotkey, kHotkeyCount> kHotkeyDefault{{
    {.modifiers = 0, .vk = 0x70},
    {.modifiers = 0, .vk = 0x71},
    {.modifiers = 0, .vk = 0x72},
    {.modifiers = 0, .vk = 0x73},
    {.modifiers = 0, .vk = 0x74},
    {.modifiers = 0, .vk = 0x75},
    {.modifiers = 0, .vk = 0x76},
    {.modifiers = 0, .vk = 0x77},
    {.modifiers = 0, .vk = 0x78},
    {.modifiers = 0, .vk = 0x79},
    {.modifiers = 0, .vk = 0x7A},
}};

// 混合计时器"末尾忙等时长"的取值范围与默认值（毫秒）。
// 这里集中定义，避免设置页和各处校验各写一份魔数。
inline constexpr int kTimerHybridSpinMsMin = 1;
inline constexpr int kTimerHybridSpinMsMax = 50;
inline constexpr int kTimerHybridSpinMsDefault = 10;

// 录制相关设置的取值范围与默认值。
//   recordMoveIntervalMs —— WH_MOUSE_LL 的移动采样间隔：小于该间隔的移动直接丢弃。
//                          0 表示不采样（全部记录），1000 表示每秒最多一条。
//   recordStartDelayMs   —— 开始录制后先等这么久再采集，用来躲开开始热键自身的按键。
//   recordMaxActions     —— 单次录制的动作条数上限，达到即自动停止。0 = 不限。
inline constexpr int kRecordMoveIntervalMsMin = 0;
inline constexpr int kRecordMoveIntervalMsMax = 1000;
inline constexpr int kRecordMoveIntervalMsDefault = 16;

inline constexpr int kRecordStartDelayMsMin = 0;
inline constexpr int kRecordStartDelayMsMax = 2000;
inline constexpr int kRecordStartDelayMsDefault = 0;

inline constexpr int kRecordMaxActionsMin = 0;
inline constexpr int kRecordMaxActionsMax = 1000000;
inline constexpr int kRecordMaxActionsDefault = 100000;

// 回放倍率：只允许这几个档位，UI 下拉与配置文件共用同一张表
inline constexpr double kPlaybackSpeeds[] = {0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0};
inline constexpr double kPlaybackSpeedDefault = 1.0;

// 把任意值归到最近的合法档位；<= 0 或 NaN 回默认档。
// 配置文件是手改的，不保证在表里，读写两侧都过一遍这个函数。
[[nodiscard]] double snapPlaybackSpeed(double speed) noexcept;

struct Config {
    // 界面语言代码（"zh-CN" / "en-US"）。
    // 空串表示"用户从未主动选择过"，启动时回退到系统语言且不写回。
    std::string language;

    TimerMode timerMode = TimerMode::Sleep;

    // 混合计时器（TimerMode::Hybrid）末尾忙等的毫秒数，范围见
    // kTimerHybridSpinMsMin/Max。其它两种模式不使用该值。
    int timerHybridSpinMs = kTimerHybridSpinMsDefault;

    // ---- 录制设置（下一次开始录制时生效）----

    // 鼠标移动采样间隔（毫秒），范围见 kRecordMoveIntervalMsMin/Max
    int recordMoveIntervalMs = kRecordMoveIntervalMsDefault;

    // 是否连其它程序（AutoHotkey、输入法等）注入的事件也不录。
    // 本程序自己注入的事件（dwExtraInfo == 'KBMR'）永远不录，与此开关无关。
    bool recordIgnoreInjected = false;

    // 开始录制前的等待（毫秒），用于过滤开始热键自身的按键
    int recordStartDelayMs = kRecordStartDelayMsDefault;

    // 单次录制的动作上限，0 = 不限
    int recordMaxActions = kRecordMaxActionsDefault;

    // 回放倍率（2.0 = 两倍速）。只影响回放前对 delayMs/holdMs 的缩放，
    // 不写进 *.macro.json
    double playbackSpeed = kPlaybackSpeedDefault;

    bool alwaysOnTop = false;

    // 上次选中的标签页索引。越界检查在 MainFrame 里做（只有它知道 tab 数量）。
    int tabIndex = 0;

    // 全局热键，顺序与 HotkeyManager 的动作表一致。
    std::array<Hotkey, kHotkeyCount> hotkeys = kHotkeyDefault;

    // 各热键的启用开关，与 hotkeys 一一对应。
    // 禁用的热键不注册、不参与冲突检测；默认全开。
    std::array<bool, kHotkeyCount> hotkeyEnabled = kHotkeyEnabledDefault;

    // 主窗口几何。
    // x/y == -1 表示"未设置"，启动时走 wxDefaultPosition。
    // 位置还会被 MainFrame 检查是否落在虚拟桌面内，屏幕外的位置会被忽略。
    struct Window {
        int x = -1;
        int y = -1;
        int width = 800;
        int height = 850;
        bool maximized = false;
    } window;

    // 零散布尔标志位，集中管理，避免顶层字段膨胀。
    struct Flags {
        // 用户已确认过"高精度 Spin 会显著提升 CPU 占用"的警告
        bool spinWarningConfirmed = false;
        // 录制脚本导入时的坐标适配偏好
        AdaptationPreference adaptationPreference = AdaptationPreference::Ask;
    } flags;
};

// ============================================================
// nlohmann ADL 序列化
//
// JSON 形态：
//   {
//     "version": 1,
//     "language": "en-US",
//     "timerMode": "sleep",
//     "timerHybridSpinMs": 10,
//     "recordMoveIntervalMs": 16,
//     "recordIgnoreInjected": false,
//     "recordStartDelayMs": 200,
//     "recordMaxActions": 100000,
//     "playbackSpeed": 1.0,
//     "alwaysOnTop": false,
//     "tabIndex": 0,
//     "hotkeys": [ {...}, ... ],
//     "hotkeyEnabled": [ true, false, ... ],
//     "window": { "x":..., "y":..., "width":..., "height":..., "maximized":... },
//     "flags": { "spinWarningConfirmed": false, "adaptationPreference": "Ask" }
//   }
//
// 兼容性策略：
//   - 缺失字段 → 默认值
//   - 多余字段 → 忽略
//   - "version" 由 ConfigPaths 在 loadFrom 时校验，不进入 Config 结构
// ============================================================

void to_json(nlohmann::json &j, const Config &c);
void from_json(const nlohmann::json &j, Config &c);
