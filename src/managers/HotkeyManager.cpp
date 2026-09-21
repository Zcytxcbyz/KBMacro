#include "managers/HotkeyManager.hpp"

#include <algorithm>
#include <utility>

#include "common/Hotkey.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"
#include "managers/ConfigManager.hpp"

namespace {

    // hk::kMod* 位掩码 → Win32 MOD_* 位掩码
    // 始终附带 MOD_NOREPEAT：按住不放只触发一次
    UINT toWin32Modifiers(const unsigned mods) {
        UINT m = MOD_NOREPEAT;
        if (mods & hk::kModCtrl)  m |= MOD_CONTROL;
        if (mods & hk::kModShift) m |= MOD_SHIFT;
        if (mods & hk::kModAlt)   m |= MOD_ALT;
        if (mods & hk::kModWin)   m |= MOD_WIN;
        return m;
    }

    // 空热键：没设置任何键。用户通过"清除"得到的状态。
    // 空热键不注册、不冲突。
    bool isEmptyHotkey(const Hotkey &hk) noexcept {
        return hk.vk == 0 && hk.modifiers == 0;
    }

} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

HotkeyManager::HotkeyManager(HWND hwnd, TriggerCallback onTriggered)
    : m_hwnd(hwnd),
      m_onTriggered(std::move(onTriggered)) {
}

HotkeyManager::~HotkeyManager() {
    unregisterAll();
}

// ============================================================
// 批量注册 / 注销
// ============================================================

void HotkeyManager::registerAll() {
    // 幂等：先清空已有注册（首次调用时是空操作）
    unregisterAll();

    const auto &hotkeys = ConfigManager::instance().config().hotkeys;
    for (int i = 0; i < kHotkeyCount; ++i) {
        registerOne(i, hotkeys[i]);
    }

    const auto registered = std::ranges::count(m_registered, true);
    LOG_INFO("hotkey: registered {}/{} (empty slots skipped)",
             registered, kHotkeyCount);
}

void HotkeyManager::unregisterAll() {
    for (int i = 0; i < kHotkeyCount; ++i) {
        unregisterOne(i);
    }
}

// ============================================================
// 单个注册 / 注销
// ============================================================

void HotkeyManager::registerOne(const int index, const Hotkey &hk) {
    if (isEmptyHotkey(hk)) {
        m_registered[index] = false;
        return;
    }

    const UINT mods = toWin32Modifiers(hk.modifiers);
    const UINT vk = hk.vk;
    const int id = kHotkeyIdBase + index;

    if (::RegisterHotKey(m_hwnd, id, mods, vk)) {
        m_registered[index] = true;
        LOG_DEBUG("hotkey[{}] registered: {}",
                  index, utils::ToUtf8(utils::hotkeyToString(hk)));
    } else {
        m_registered[index] = false;
        LOG_WARN("hotkey[{}] register failed: {} (err={})",
                 index, utils::ToUtf8(utils::hotkeyToString(hk)),
                 ::GetLastError());
    }
}

void HotkeyManager::unregisterOne(const int index) {
    if (!m_registered[index]) return;

    const int id = kHotkeyIdBase + index;
    if (::UnregisterHotKey(m_hwnd, id)) {
        LOG_DEBUG("hotkey[{}] unregistered", index);
    } else {
        LOG_WARN("hotkey[{}] unregister failed (err={})",
                 index, ::GetLastError());
    }
    m_registered[index] = false;
}

// ============================================================
// 修改单个热键
// ============================================================

bool HotkeyManager::setHotkey(const int index, const Hotkey &hk) {
    if (index < 0 || index >= kHotkeyCount) {
        LOG_WARN("hotkey: setHotkey index out of range: {}", index);
        return false;
    }

    // 冲突检测：空热键不参与
    if (!isEmptyHotkey(hk) && isConflicting(hk, index)) {
        LOG_WARN("hotkey[{}] conflicts with another hotkey, rejected", index);
        return false;
    }

    // 注销旧的（若未注册则空操作）
    unregisterOne(index);

    // 落盘。即便下一步注册失败，也保留用户输入，由 failedIndices 反馈。
    ConfigManager::instance().setHotkey(index, hk);

    // 注册新的（空热键只落盘，不注册）
    registerOne(index, hk);

    // 空热键视为"清除成功"，注册成功也返回 true
    return m_registered[index] || isEmptyHotkey(hk);
}

// ============================================================
// 分发
// ============================================================

// ReSharper disable once CppMemberFunctionMayBeConst
void HotkeyManager::dispatch(const int hotkeyId) {
    const int index = hotkeyId - kHotkeyIdBase;
    if (index < 0 || index >= kHotkeyCount) {
        LOG_WARN("hotkey: dispatch invalid id: {}", hotkeyId);
        return;
    }
    if (!m_registered[index]) {
        LOG_WARN("hotkey: dispatch for unregistered slot {}", index);
        return;
    }

    LOG_DEBUG("hotkey[{}] triggered", index);
    if (m_onTriggered) {
        m_onTriggered(static_cast<HotkeyAction>(index));
    }
}

// ============================================================
// 查询
// ============================================================

int HotkeyManager::failedCount() const noexcept {
    const auto &hotkeys = ConfigManager::instance().config().hotkeys;
    int n = 0;
    for (int i = 0; i < kHotkeyCount; ++i) {
        // 只统计"用户设置过但注册失败"的项；空热键不算失败
        if (!m_registered[i] && !isEmptyHotkey(hotkeys[i])) {
            ++n;
        }
    }
    return n;
}

std::vector<int> HotkeyManager::failedIndices() const {
    const auto &hotkeys = ConfigManager::instance().config().hotkeys;
    std::vector<int> result;
    result.reserve(kHotkeyCount);

    for (int i = 0; i < kHotkeyCount; ++i) {
        if (!m_registered[i] && !isEmptyHotkey(hotkeys[i])) {
            result.push_back(i);
        }
    }
    return result;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
bool HotkeyManager::isConflicting(const Hotkey &hk, // NOLINT(*-convert-member-functions-to-static)
                                  const int excludeIndex) const {
    if (isEmptyHotkey(hk)) return false;

    const auto &hotkeys = ConfigManager::instance().config().hotkeys;
    for (int i = 0; i < kHotkeyCount; ++i) {
        if (i == excludeIndex) continue;
        if (hotkeys[i].vk == hk.vk && hotkeys[i].modifiers == hk.modifiers) {
            return true;
        }
    }
    return false;
}
