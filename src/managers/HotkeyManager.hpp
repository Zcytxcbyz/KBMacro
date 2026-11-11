#pragma once

#include <array>
#include <functional>
#include <vector>

#include <windows.h>

#include "common/HotkeyAction.hpp"
#include "common/Types.hpp"

// ============================================================
// Global hotkey manager
//
// Responsibilities:
//   - hold hotkey registration state (which entries registered successfully)
//   - register the hotkeys from Config in bulk at startup
//   - modify a single hotkey: conflict check → unregister old → persist → register new
//   - dispatch WM_HOTKEY to the trigger callback by id
//
// Not responsible for:
//   - UI work (dialogs and the status bar belong to MainFrame)
//   - binding wxEVT_HOTKEY (no window here, MainFrame does the binding)
//   - owning Config::hotkeys (data lives in Config, this class only reads)
//
// Lifetime:
//   - MainFrame member (unique_ptr), HWND and trigger callback injected on construction
//   - unregister all registered hotkeys on destruction
//
// Threading:
//   - used on the UI thread only (RegisterHotKey / UnregisterHotKey / dispatch all on the UI thread)
// ============================================================

class HotkeyManager {
public:
    // hotkey trigger callback: called with the action looked up by index during dispatch
    using TriggerCallback = std::function<void(HotkeyAction)>;

    // id base for WM_HOTKEY: id = kHotkeyIdBase + index (index ∈ [0, kHotkeyCount))
    static constexpr int kHotkeyIdBase = 0x1000;

    HotkeyManager(HWND hwnd, TriggerCallback onTriggered);
    ~HotkeyManager();

    // copying / moving is deleted: owns HWND and registration state, no value semantics
    HotkeyManager(const HotkeyManager &) = delete;
    HotkeyManager &operator=(const HotkeyManager &) = delete;
    HotkeyManager(HotkeyManager &&) = delete;
    HotkeyManager &operator=(HotkeyManager &&) = delete;

    // call once at startup: walk ConfigManager::config().hotkeys and register each
    void registerAll();

    // unregister all registered hotkeys (also called from the destructor, idempotent)
    void unregisterAll();

    // modify one hotkey: conflict check → unregister old → persist (ConfigManager) → register new
    //   returns true  = success
    //   returns false = failure (conflict / taken by the system or another program / index out of range)
    //
    // a "disabled" hotkey only gets its binding changed: stays disabled, not registered, always true.
    //
    // on failure:
    //   - conflict: nothing persisted, nothing registered, the old value in Config is unchanged
    //   - registration failed: already persisted, but m_registered[index] = false, see failedIndices
    bool setHotkey(int index, const Hotkey &hk);

    // change the enabled state of one hotkey.
    //   enabled == true  → persist + register (fails if taken by the system / another program)
    //   enabled == false → persist + unregister
    //   returns true  = operation succeeded (disable always succeeds)
    //   returns false = index out of range, or registration failed while enabling
    //
    // Note: like setHotkey, the user intent is persisted even when registration fails,
    // the next start retries via registerAll.
    bool setEnabled(int index, bool enabled);

    // forwarded here by MainFrame after it receives wxEVT_HOTKEY
    // id comes from wxKeyEvent::GetId()
    void dispatch(int hotkeyId);

    // ---- queries (used by MainFrame to summarize the status bar) ----

    // number of hotkeys that failed to register
    [[nodiscard]] int failedCount() const noexcept;

    // indices of the hotkeys that failed to register
    [[nodiscard]] std::vector<int> failedIndices() const;

    // conflict check: whether hk equals any hotkey other than excludeIndex
    // excludeIndex skips the entry itself when "editing item N", pass -1 to exclude nothing
    [[nodiscard]] bool isConflicting(const Hotkey &hk, int excludeIndex) const;

private:
    // register a single hotkey; on failure write LOG_WARN and update m_registered
    void registerOne(int index, const Hotkey &hk);

    // unregister a single hotkey; only call UnregisterHotKey when m_registered[index] is true
    void unregisterOne(int index);

    HWND m_hwnd = nullptr;
    TriggerCallback m_onTriggered;

    // m_registered[i] = whether hotkey i registered successfully
    std::array<bool, kHotkeyCount> m_registered{};
};
