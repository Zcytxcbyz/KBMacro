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

namespace {

// Every key on a US 104-key layout, left and right modifiers both included.
std::vector<unsigned> usKeyboardKeys() {
    std::vector<unsigned> keys;
    for (unsigned c = 'A'; c <= 'Z'; ++c)
        keys.push_back(c);
    for (unsigned c = '0'; c <= '9'; ++c)
        keys.push_back(c);
    for (unsigned vk = VK_F1; vk <= VK_F12; ++vk)
        keys.push_back(vk);
    for (unsigned vk = VK_NUMPAD0; vk <= VK_NUMPAD9; ++vk)
        keys.push_back(vk);

    keys.insert(keys.end(),
                {VK_ESCAPE,   VK_TAB,      VK_CAPITAL,  VK_SPACE,    VK_BACK,      VK_RETURN,    VK_INSERT,
                 VK_DELETE,   VK_HOME,     VK_END,      VK_PRIOR,    VK_NEXT,      VK_LEFT,      VK_UP,
                 VK_RIGHT,    VK_DOWN,     VK_SNAPSHOT, VK_SCROLL,   VK_PAUSE,     VK_NUMLOCK,   VK_MULTIPLY,
                 VK_ADD,      VK_SUBTRACT, VK_DECIMAL,  VK_DIVIDE,   VK_LSHIFT,    VK_RSHIFT,    VK_SHIFT,
                 VK_LCONTROL, VK_RCONTROL, VK_CONTROL,  VK_LMENU,    VK_RMENU,     VK_MENU,      VK_LWIN,
                 VK_RWIN,     VK_APPS,     VK_OEM_1,    VK_OEM_PLUS, VK_OEM_COMMA, VK_OEM_MINUS, VK_OEM_PERIOD,
                 VK_OEM_2,    VK_OEM_3,    VK_OEM_4,    VK_OEM_5,    VK_OEM_6,     VK_OEM_7});
    return keys;
}

// Right-hand modifiers fold into the generic (left) vk, because the app does not model sides.
unsigned canonicalVk(const unsigned vk) {
    switch (vk) {
        case VK_LCONTROL:
        case VK_RCONTROL:
            return VK_CONTROL;
        case VK_LSHIFT:
        case VK_RSHIFT:
            return VK_SHIFT;
        case VK_LMENU:
        case VK_RMENU:
            return VK_MENU;
        case VK_RWIN:
            return VK_LWIN;
        default:
            return vk;
    }
}

} // namespace

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
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_LCONTROL)), "Ctrl");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_RCONTROL)), "Ctrl");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_SHIFT)), "Shift");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_RSHIFT)), "Shift");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_MENU)), "Alt");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_RMENU)), "Alt");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_RWIN)), "Win");
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
    EXPECT_EQ(utils::vkFromString(L"Shift"), static_cast<unsigned>(VK_SHIFT));
    EXPECT_EQ(utils::vkFromString(L"Alt"), static_cast<unsigned>(VK_MENU));
    EXPECT_EQ(utils::vkFromString(L"Win"), static_cast<unsigned>(VK_LWIN));
}

TEST(VkFromString, WholeTableRoundTrip) {
    for (const auto &[vk, name] : kKeyNames) {
        ASSERT_NE(name, nullptr);
        EXPECT_EQ(utils::vkFromString(name), vk) << "name=" << utils::ToUtf8(name);
        EXPECT_EQ(utils::ToUtf8(utils::vkToString(vk)), utils::ToUtf8(name)) << "vk=" << vk;
    }
}

TEST(VkToString, UsKeyboardLayoutRoundTrips) {
    for (const unsigned vk : usKeyboardKeys()) {
        const auto name = utils::vkToString(vk);
        ASSERT_FALSE(name.empty()) << "vk=" << vk;
        ASSERT_NE(name, L"?") << "no display name for vk=" << vk;

        // vk -> name -> vk: right-hand modifiers are expected to come back as the generic one
        EXPECT_EQ(utils::vkFromString(name), canonicalVk(vk)) << "vk=" << vk << " name=" << utils::ToUtf8(name);
    }
}

TEST(VkFromString, UsKeyboardNamesAreDistinct) {
    std::map<std::wstring, unsigned> seen;
    for (const unsigned vk : usKeyboardKeys()) {
        const auto name = utils::vkToString(vk);
        const auto [it, inserted] = seen.try_emplace(name, vk);
        if (inserted) continue;

        // only the left/right modifier variants may share a name
        EXPECT_TRUE(utils::isModifierKey(vk) && utils::isModifierKey(it->second))
                << "duplicate name " << utils::ToUtf8(name) << " for vk=" << vk << " and vk=" << it->second;
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
