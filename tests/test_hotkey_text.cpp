// ============================================================
// Hotkey text / validation tests
//
// covers the pure logic in utils for hotkey display, parsing and validity:
//   1. vkToString / vkFromString: known keys, unknown keys, empty values
//   2. hotkeyToString: fixed modifier order + main key concatenation
//   3. isModifierKey / modifierBitForVk: modifier lookup tables
//   4. isValidHotkey: two strictness levels (whether a main key is required)
//
// Key names come from common/KeyNames.gen.hpp (generated); the expectations pin
// the display names every user sees after release -- if regenerating the key
// table changes them, these cases fail and remind you to check the UI strings.
//
// Comparisons always go through ToUtf8 into std::string first: on failure gtest
// prints readable UTF-8 instead of a heap of wide-char bytes.
// ============================================================

#include <gtest/gtest.h>

#include <windows.h>

#include "common/Hotkey.hpp"
#include "common/KeyNames.gen.hpp"
#include "common/Utils.hpp"

// ---- vkToString ----

TEST(VkToString, ZeroIsEmpty) {
    EXPECT_TRUE(utils::vkToString(0).empty());
}

TEST(VkToString, KnownKeys) {
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_F1)), "F1");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_ESCAPE)), "Esc");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_RETURN)), "Enter");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_SPACE)), "Space");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_LEFT)), "Left");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString('A')), "A");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString('0')), "0");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_OEM_1)), ";");
}

TEST(VkToString, UnknownKeyIsQuestionMark) {
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(0x07)), "?");
}

TEST(VkToString, ModifierKeys) {
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_CONTROL)), "Ctrl");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_LCONTROL)), "Left Ctrl");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_SHIFT)), "Shift");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_RSHIFT)), "Right Shift");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_MENU)), "Alt");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_RMENU)), "Right Alt");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_LWIN)), "Win");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_RWIN)), "Right Win");
}

// ---- vkFromString ----

TEST(VkFromString, EmptyAndUnknownAreZero) {
    EXPECT_EQ(utils::vkFromString(L""), 0u);
    EXPECT_EQ(utils::vkFromString(L"?"), 0u);
    EXPECT_EQ(utils::vkFromString(L"No Such Key"), 0u);
}

TEST(VkFromString, KnownNames) {
    EXPECT_EQ(utils::vkFromString(L"F1"), static_cast<unsigned>(VK_F1));
    EXPECT_EQ(utils::vkFromString(L"Esc"), static_cast<unsigned>(VK_ESCAPE));
    EXPECT_EQ(utils::vkFromString(L"A"), static_cast<unsigned>('A'));
    EXPECT_EQ(utils::vkFromString(L"Page Up"), static_cast<unsigned>(VK_PRIOR));
    // case-sensitive: the display name is the only accepted spelling
    EXPECT_EQ(utils::vkFromString(L"f1"), 0u);
    EXPECT_EQ(utils::vkFromString(L"esc"), 0u);
}

TEST(VkFromString, ModifierNames) {
    EXPECT_EQ(utils::vkFromString(L"Ctrl"), static_cast<unsigned>(VK_CONTROL));
    EXPECT_EQ(utils::vkFromString(L"Right Shift"), static_cast<unsigned>(VK_RSHIFT));
    EXPECT_EQ(utils::vkFromString(L"Win"), static_cast<unsigned>(VK_LWIN));
}

TEST(VkFromString, WholeTableRoundTrip) {
    for (const auto &[vk, name] : kKeyNames) {
        ASSERT_NE(name, nullptr);
        EXPECT_EQ(utils::vkFromString(name), vk) << "name=" << utils::ToUtf8(name);
        EXPECT_EQ(utils::ToUtf8(utils::vkToString(vk)), utils::ToUtf8(name)) << "vk=" << vk;
    }
}

// ---- hotkeyToString ----

TEST(HotkeyToString, EmptyHotkeyIsEmptyString) {
    EXPECT_TRUE(utils::hotkeyToString(Hotkey{}).empty());
}

TEST(HotkeyToString, ModifiersOnly) {
    Hotkey hk;
    hk.modifiers = hk::kModCtrl | hk::kModAlt;
    EXPECT_EQ(utils::ToUtf8(utils::hotkeyToString(hk)), "Ctrl+Alt");
}

TEST(HotkeyToString, ModifierOrderIsFixed) {
    Hotkey hk;
    hk.modifiers = hk::kModWin | hk::kModCtrl; // bit order does not affect display order
    EXPECT_EQ(utils::ToUtf8(utils::hotkeyToString(hk)), "Ctrl+Win");
}

TEST(HotkeyToString, FullCombination) {
    Hotkey hk;
    hk.modifiers = hk::kModCtrl | hk::kModShift | hk::kModAlt | hk::kModWin;
    hk.vk = VK_F5;
    EXPECT_EQ(utils::ToUtf8(utils::hotkeyToString(hk)), "Ctrl+Shift+Alt+Win+F5");
}

TEST(HotkeyToString, MainKeyOnly) {
    Hotkey hk;
    hk.vk = VK_SPACE;
    EXPECT_EQ(utils::ToUtf8(utils::hotkeyToString(hk)), "Space");
}

TEST(HotkeyToString, UnknownMainKeyShowsQuestionMark) {
    Hotkey hk;
    hk.modifiers = hk::kModShift;
    hk.vk = 0x07;
    EXPECT_EQ(utils::ToUtf8(utils::hotkeyToString(hk)), "Shift+?");
}

// ---- isModifierKey ----

TEST(IsModifierKey, EveryModifierVariant) {
    constexpr unsigned kModifierVks[] = {
            VK_CONTROL,
            VK_LCONTROL,
            VK_RCONTROL,
            VK_SHIFT,
            VK_LSHIFT,
            VK_RSHIFT,
            VK_MENU,
            VK_LMENU,
            VK_RMENU,
            VK_LWIN,
            VK_RWIN,
    };
    for (const unsigned vk : kModifierVks) {
        EXPECT_TRUE(utils::isModifierKey(vk)) << "vk=" << vk;
    }
}

TEST(IsModifierKey, OrdinaryKeys) {
    constexpr unsigned kOrdinary[] = {0, 'A', '0', VK_F1, VK_ESCAPE, VK_SPACE, VK_TAB};
    for (const unsigned vk : kOrdinary) {
        EXPECT_FALSE(utils::isModifierKey(vk)) << "vk=" << vk;
    }
}

// ---- modifierBitForVk ----

TEST(ModifierBitForVk, MapsEveryVariant) {
    EXPECT_EQ(utils::modifierBitForVk(VK_LCONTROL), hk::kModCtrl);
    EXPECT_EQ(utils::modifierBitForVk(VK_RCONTROL), hk::kModCtrl);
    EXPECT_EQ(utils::modifierBitForVk(VK_LSHIFT), hk::kModShift);
    EXPECT_EQ(utils::modifierBitForVk(VK_RSHIFT), hk::kModShift);
    EXPECT_EQ(utils::modifierBitForVk(VK_LMENU), hk::kModAlt);
    EXPECT_EQ(utils::modifierBitForVk(VK_RMENU), hk::kModAlt);
    EXPECT_EQ(utils::modifierBitForVk(VK_LWIN), hk::kModWin);
    EXPECT_EQ(utils::modifierBitForVk(VK_RWIN), hk::kModWin);
}

TEST(ModifierBitForVk, NonModifierIsZero) {
    EXPECT_EQ(utils::modifierBitForVk('A'), 0u);
    EXPECT_EQ(utils::modifierBitForVk(VK_F1), 0u);
    EXPECT_EQ(utils::modifierBitForVk(0), 0u);
}

// ---- isValidHotkey ----

TEST(HotkeyValidity, EmptyIsNeverValid) {
    EXPECT_FALSE(utils::isValidHotkey(Hotkey{}, false));
    EXPECT_FALSE(utils::isValidHotkey(Hotkey{}, true));
}

TEST(HotkeyValidity, ModifiersOnly) {
    Hotkey hk;
    hk.modifiers = hk::kModCtrl;
    EXPECT_TRUE(utils::isValidHotkey(hk, false)); // the panel allows modifier-only hotkeys
    EXPECT_FALSE(utils::isValidHotkey(hk, true)); // global hotkeys must have a main key
}

TEST(HotkeyValidity, MainKeyOnly) {
    Hotkey hk;
    hk.vk = 'A';
    EXPECT_TRUE(utils::isValidHotkey(hk, false));
    EXPECT_TRUE(utils::isValidHotkey(hk, true));
}
