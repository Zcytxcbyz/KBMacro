#pragma once

#include <memory>

#include <wx/wx.h>

#include "common/HotkeyAction.hpp"
#include "common/ITimer.hpp"

// forward declarations (avoid pulling a pile of panel implementations into the header)
class wxNotebook;
class HotkeyManager;
class KBMacroStatusBar;
class ClickerPanel;
class KeyboardPanel;
class RecorderPanel;
class SettingsPanel;
class AboutPanel;
class ThreadManager;

// main window.
//
// responsibilities:
//   - create and own the wxNotebook, assembling the five tabs
//   - own the HotkeyManager, receive WM_HOTKEY and dispatch to the matching panel
//   - handle window-level style, size, position and tab memory
//
// hotkey dispatch route:
//   HotkeyManager::TriggerCallback -> MainFrame::dispatchHotkeyAction
//     -> each panel's startWithCurrentParams() / stop() / ...
//   MainFrame does not touch ThreadManager directly; panels do (once the module lands).
class MainFrame : public wxFrame {
public:
    MainFrame();
    ~MainFrame() override;

    // Write window geometry + current tab to ConfigManager.
    // Extracted from onClose so "restart to switch language" paths that bypass wxEVT_CLOSE_WINDOW
    // can call it explicitly.
    void saveWindowState();

    // Rebuild the timer instance from the current config. Called when the settings tab switches
    // timer mode or changes the trailing busy-wait duration.
    // The old instance stays alive via shared_ptr: running tasks finish on it, later tasks use the new one.
    void rebuildTimer();

    // Re-label the panels' hotkey-driven buttons with the current keys ("Start (F1)").
    // Called after the hotkey list changes on the settings tab.
    void refreshHotkeyHints();

private:
    // HotkeyManager callback entry: switch to the matching panel method
    void dispatchHotkeyAction(HotkeyAction action);

    // wxEVT_HOTKEY handler: forward to HotkeyManager::dispatch
    void onHotkey(wxKeyEvent &event);

    // wxEVT_CLOSE_WINDOW handler: save window geometry + current tab, then allow the close
    void onClose(wxCloseEvent &event);

    // write the failure count to the status bar when registration fails
    void reportHotkeyFailures();

    void refreshStatus();
    void onStatusTimer(wxTimerEvent &event);

    // ---- timer ----
    // declaration order matters: during wiring, m_threadMgr holds a reference to this shared_ptr,
    // so it must come before m_threadMgr (destruction is reversed: stop threads, then drop the timer).
    std::shared_ptr<ITimer> m_timer;

    // ---- threads and tasks ----
    // must be declared after m_timer: destruction is reversed, stop threads before dropping
    // the timer (threads still hold a timer reference)
    std::unique_ptr<ThreadManager> m_threadMgr;

    // ---- services ----
    std::unique_ptr<HotkeyManager> m_hotkeyMgr;

    // ---- layout ----
    wxNotebook *m_notebook{};

    // ---- panels (hotkey dispatch targets) ----
    ClickerPanel *m_clickerPanel{};
    KeyboardPanel *m_keyboardPanel{};
    RecorderPanel *m_recorderPanel{};
    SettingsPanel *m_settingsPanel{};
    AboutPanel *m_aboutPanel{};

    // ---- status bar ----
    KBMacroStatusBar *m_statusBar{};
    wxTimer m_statusTimer{this}; // poll task status every 300ms, driving the status bar's three states
};
