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

struct Config {
    // 界面语言代码（"zh-CN" / "en-US"）。
    // 空串表示"用户从未主动选择过"，启动时回退到系统语言且不写回。
    std::string language;

    TimerMode timerMode = TimerMode::Sleep;

    bool alwaysOnTop = false;

    // 上次选中的标签页索引。越界检查在 MainFrame 里做（只有它知道 tab 数量）。
    int tabIndex = 0;

    // 全局热键，顺序与 HotkeyManager 的动作表一致。
    std::array<Hotkey, kHotkeyCount> hotkeys{};

    // 各热键的启用开关，与 hotkeys 一一对应。
    // 禁用的热键不注册、不参与冲突检测；默认全开。
    std::array<bool, kHotkeyCount> hotkeyEnabled = kHotkeyEnabledDefault;

    // 主窗口几何。
    // x/y == -1 表示"未设置"，启动时走 wxDefaultPosition。
    // 位置还会被 MainFrame 检查是否落在虚拟桌面内，屏幕外的位置会被忽略。
    struct Window {
        int x = -1;
        int y = -1;
        int width = 650;
        int height = 720;
        bool maximized = false;
    } window;

    // 零散布尔标志位，集中管理，避免顶层字段膨胀。
    struct Flags {
        // 用户已确认过"高精度 Spin 会显著提升 CPU 占用"的警告
        bool spinWarningConfirmed = false;
    } flags;
};

// ============================================================
// nlohmann ADL 序列化
//
// JSON 形态：
//   {
//     "version": 1,
//     "language": "zh-CN",
//     "timerMode": "sleep",
//     "alwaysOnTop": false,
//     "tabIndex": 0,
//     "hotkeys": [ {...}, ... ],
//     "hotkeyEnabled": [ true, false, ... ],
//     "window": { "x":..., "y":..., "width":..., "height":..., "maximized":... },
//     "flags": { "spinWarningConfirmed": false }
//   }
//
// 兼容性策略：
//   - 缺失字段 → 默认值
//   - 多余字段 → 忽略
//   - "version" 由 ConfigPaths 在 loadFrom 时校验，不进入 Config 结构
// ============================================================

void to_json(nlohmann::json &j, const Config &c);
void from_json(const nlohmann::json &j, Config &c);
