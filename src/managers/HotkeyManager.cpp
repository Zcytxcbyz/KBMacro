#include "managers/HotkeyManager.hpp"

#include <algorithm>
#include <utility>

#include "common/Hotkey.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"
#include "managers/ConfigManager.hpp"

namespace {

    // hk::kMod* bitmask → Win32 MOD_* bitmask
    // MOD_NOREPEAT is always added: holding the key triggers only once
    UINT toWin32Modifiers(const unsigned mods) {
        UINT m = MOD_NOREPEAT;
        if (mods & hk::kModCtrl)  m |= MOD_CONTROL;
        if (mods & hk::kModShift) m |= MOD_SHIFT;
        if (mods & hk::kModAlt)   m |= MOD_ALT;
        if (mods & hk::kModWin)   m |= MOD_WIN;
        return m;
    }

    // empty hotkey: no key assigned. The state the user gets via "clear".
    // empty hotkeys are not registered and never conflict.
    bool isEmptyHotkey(const Hotkey &hk) noexcept {
        return hk.vk == 0 && hk.modifiers == 0;
    }

} // namespace

// ============================================================
// Construction / destruction
// ============================================================

HotkeyManager::HotkeyManager(HWND hwnd, TriggerCallback onTriggered)
    : m_hwnd(hwnd),
      m_onTriggered(std::move(onTriggered)) {
}

HotkeyManager::~HotkeyManager() {
    unregisterAll();
}

// ============================================================
// Bulk register / unregister
// ============================================================

void HotkeyManager::registerAll() {
    // idempotent: clear existing registrations first (a no-op on the first call)
    unregisterAll();

    const auto &cfg = ConfigManager::instance().config();
    for (int i = 0; i < kHotkeyCount; ++i) {
        if (!cfg.hotkeyEnabled[static_cast<std::size_t>(i)]) {
            // disabled slots are not registered and do not count as failures
            continue;
        }
        registerOne(i, cfg.hotkeys[i]);
    }

    const auto registered = std::ranges::count(m_registered, true);
    LOG_INFO("hotkey: registered {}/{} (empty or disabled slots skipped)",
             registered, kHotkeyCount);
}

void HotkeyManager::unregisterAll() {
    for (int i = 0; i < kHotkeyCount; ++i) {
        unregisterOne(i);
    }
}

// ============================================================
// Single register / unregister
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
// Modify a single hotkey
// ============================================================

bool HotkeyManager::setHotkey(const int index, const Hotkey &hk) {
    if (index < 0 || index >= kHotkeyCount) {
        LOG_WARN("hotkey: setHotkey index out of range: {}", index);
        return false;
    }

    // conflict check: empty hotkeys do not participate
    if (!isEmptyHotkey(hk) && isConflicting(hk, index)) {
        LOG_WARN("hotkey[{}] conflicts with another hotkey, rejected", index);
        return false;
    }

    // unregister the old one (a no-op when not registered)
    unregisterOne(index);

    // persist. Even if registration fails next, keep the user input and report it via failedIndices.
    ConfigManager::instance().setHotkey(index, hk);

    // rebinding while disabled: only persist, stay disabled, do not register
    const bool enabled = ConfigManager::instance()
                             .config()
                             .hotkeyEnabled[static_cast<std::size_t>(index)];
    if (!enabled) {
        m_registered[index] = false;
        LOG_DEBUG("hotkey[{}] updated but stays disabled", index);
        return true;
    }

    // register the new one (an empty hotkey is only persisted, not registered)
    registerOne(index, hk);

    // an empty hotkey counts as "cleared", a successful registration also returns true
    return m_registered[index] || isEmptyHotkey(hk);
}

bool HotkeyManager::setEnabled(const int index, const bool enabled) {
    if (index < 0 || index >= kHotkeyCount) {
        LOG_WARN("hotkey: setEnabled index out of range: {}", index);
        return false;
    }

    // persist first (record user intent), then touch the registration state
    ConfigManager::instance().setHotkeyEnabled(index, enabled);

    if (!enabled) {
        unregisterOne(index);
        LOG_DEBUG("hotkey[{}] disabled", index);
        return true;
    }

    const auto &hk = ConfigManager::instance()
                         .config()
                         .hotkeys[static_cast<std::size_t>(index)];
    registerOne(index, hk);
    return m_registered[index] || isEmptyHotkey(hk);
}

// ============================================================
// Dispatch
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
// Queries
// ============================================================

int HotkeyManager::failedCount() const noexcept {
    const auto &cfg = ConfigManager::instance().config();
    int n = 0;
    for (int i = 0; i < kHotkeyCount; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        if (!cfg.hotkeyEnabled[idx]) continue;   // ← added: disabled slots do not count as failures
        // only count items "set by the user but failed to register"; empty hotkeys do not count
        if (!m_registered[i] && !isEmptyHotkey(cfg.hotkeys[idx])) {
            ++n;
        }
    }
    return n;
}

std::vector<int> HotkeyManager::failedIndices() const {
    const auto &cfg = ConfigManager::instance().config();
    std::vector<int> result;
    result.reserve(kHotkeyCount);

    for (int i = 0; i < kHotkeyCount; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        if (!cfg.hotkeyEnabled[idx]) continue;   // ← added
        if (!m_registered[i] && !isEmptyHotkey(cfg.hotkeys[idx])) {
            result.push_back(i);
        }
    }
    return result;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
bool HotkeyManager::isConflicting(const Hotkey &hk, // NOLINT(*-convert-member-functions-to-static)
                                  const int excludeIndex) const {
    if (isEmptyHotkey(hk)) return false;

    const auto &cfg = ConfigManager::instance().config();
    for (int i = 0; i < kHotkeyCount; ++i) {
        if (i == excludeIndex) continue;
        const auto idx = static_cast<std::size_t>(i);
        if (!cfg.hotkeyEnabled[idx]) continue;   // ← added: disabled slots never conflict
        if (cfg.hotkeys[idx].vk == hk.vk &&
            cfg.hotkeys[idx].modifiers == hk.modifiers) {
            return true;
            }
    }
    return false;
}
