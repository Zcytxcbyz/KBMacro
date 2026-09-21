#pragma once

#include <array>
#include <functional>
#include <vector>

#include <windows.h>

#include "common/HotkeyAction.hpp"
#include "common/Types.hpp"

// ============================================================
// 全局热键管理器
//
// 职责：
//   - 持有热键注册状态（哪几项注册成功）
//   - 启动时批量注册 Config 里的热键
//   - 修改单个热键：冲突检测 → 注销旧的 → 落盘 → 注册新的
//   - 收到 WM_HOTKEY 后按 id 分发到触发回调
//
// 不做：
//   - 不操作 UI（弹窗、状态栏由 MainFrame 负责）
//   - 不绑定 wxEVT_HOTKEY（没有窗口，绑定由 MainFrame 做）
//   - 不拥有 Config::hotkeys 的所有权（数据在 Config，本类只读）
//
// 生命周期：
//   - MainFrame 成员（unique_ptr），构造时注入 HWND 与触发回调
//   - 析构时注销所有已注册的热键
//
// 线程：
//   - 只在 UI 线程使用（RegisterHotKey / UnregisterHotKey / dispatch 都在 UI 线程）
// ============================================================

class HotkeyManager {
public:
    // 热键触发回调：dispatch 时按动作索引查表后调用
    using TriggerCallback = std::function<void(HotkeyAction)>;

    // WM_HOTKEY 的 id 基址：id = kHotkeyIdBase + index（index ∈ [0, kHotkeyCount)）
    static constexpr int kHotkeyIdBase = 0x1000;

    HotkeyManager(HWND hwnd, TriggerCallback onTriggered);
    ~HotkeyManager();

    // 禁止拷贝 / 移动：持 HWND 和注册状态，语义上没有值语义
    HotkeyManager(const HotkeyManager &) = delete;
    HotkeyManager &operator=(const HotkeyManager &) = delete;
    HotkeyManager(HotkeyManager &&) = delete;
    HotkeyManager &operator=(HotkeyManager &&) = delete;

    // 启动时调一次：遍历 ConfigManager::config().hotkeys，逐个注册
    void registerAll();

    // 注销所有已注册的热键（析构内部也会调，幂等）
    void unregisterAll();

    // 修改某个热键：冲突检测 → 注销旧的 → 落盘（ConfigManager）→ 注册新的
    //   返回 true  = 成功
    //   返回 false = 失败（冲突 / 注册被系统或其他程序占用 / index 越界）
    //
    // 若该热键当前处于"禁用"状态：只改键位、保持禁用，不注册，恒返回 true。
    //
    // 失败时：
    //   - 冲突：不落盘、不注册，Config 里的旧值保持不变
    //   - 注册失败：已落盘，但 m_registered[index] = false，可由 failedIndices 查询
    bool setHotkey(int index, const Hotkey &hk);

    // 修改某个热键的启用状态。
    //   enabled == true  → 落盘 + 注册（可能因被系统/其他程序占用而失败）
    //   enabled == false → 落盘 + 注销
    //   返回 true  = 操作成功（disable 恒成功）
    //   返回 false = index 越界，或 enable 时注册失败
    //
    // 注意：与 setHotkey 一样，注册失败时用户意图已落盘，
    // 下次启动 registerAll 会重试。
    bool setEnabled(int index, bool enabled);

    // MainFrame 收到 wxEVT_HOTKEY 后转发进来
    // id 来自 wxKeyEvent::GetId()
    void dispatch(int hotkeyId);

    // ---- 查询（MainFrame 状态栏汇总用）----

    // 注册失败的热键数量
    [[nodiscard]] int failedCount() const noexcept;

    // 注册失败的热键索引列表
    [[nodiscard]] std::vector<int> failedIndices() const;

    // 冲突检测：hk 是否与除 excludeIndex 外的其他热键相同
    // excludeIndex 用于"修改第 N 项"时跳过自身，传 -1 表示不排除任何项
    [[nodiscard]] bool isConflicting(const Hotkey &hk, int excludeIndex) const;

private:
    // 注册单个热键；失败写 LOG_WARN，更新 m_registered
    void registerOne(int index, const Hotkey &hk);

    // 注销单个热键；只在 m_registered[index] 为 true 时才调 UnregisterHotKey
    void unregisterOne(int index);

    HWND m_hwnd = nullptr;
    TriggerCallback m_onTriggered;

    // m_registered[i] = 第 i 项热键是否注册成功
    std::array<bool, kHotkeyCount> m_registered{};
};
