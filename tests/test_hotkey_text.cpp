// ============================================================
// 热键文本化 / 校验测试
//
// 覆盖 utils 里与热键显示、解析、合法性相关的纯逻辑：
//   1. vkToString / vkFromString：已知键、未知键、空值
//   2. hotkeyToString：修饰键固定顺序 + 主键拼接
//   3. isModifierKey / modifierBitForVk：修饰键判定表
//   4. isValidHotkey：两种严格度（是否要求主键）
//
// 键名来自 common/KeyNames.gen.hpp（生成物），断言里写死的是
// 发布后所有用户看到的显示名——若重新生成键名表导致名字变化，
// 这些用例会失败，提醒同步检查 UI 文案有没有跟着改。
//
// 字符串比较统一走 ToUtf8 转成 std::string 再做：失败时 gtest
// 打出来的是可读的 UTF-8，不会变成宽字符的字节堆。
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
    // 修饰键本身不进键名表：它们走 modifiers 位，不参与 vk 文本化
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_LCONTROL)), "?");
    EXPECT_EQ(utils::ToUtf8(utils::vkToString(VK_RWIN)), "?");
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
    // 大小写敏感：显示名就是唯一合法写法
    EXPECT_EQ(utils::vkFromString(L"f1"), 0u);
    EXPECT_EQ(utils::vkFromString(L"esc"), 0u);
}

TEST(VkFromString, WholeTableRoundTrip) {
    for (const auto &[vk, name]: kKeyNames) {
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
    hk.modifiers = hk::kModWin | hk::kModCtrl;   // 位顺序不影响显示顺序
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
        VK_CONTROL, VK_LCONTROL, VK_RCONTROL,
        VK_SHIFT,   VK_LSHIFT,   VK_RSHIFT,
        VK_MENU,    VK_LMENU,    VK_RMENU,
        VK_LWIN,    VK_RWIN,
    };
    for (const unsigned vk: kModifierVks) {
        EXPECT_TRUE(utils::isModifierKey(vk)) << "vk=" << vk;
    }
}

TEST(IsModifierKey, OrdinaryKeys) {
    constexpr unsigned kOrdinary[] = {0, 'A', '0', VK_F1, VK_ESCAPE, VK_SPACE, VK_TAB};
    for (const unsigned vk: kOrdinary) {
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
    EXPECT_TRUE(utils::isValidHotkey(hk, false));   // 面板允许纯修饰键
    EXPECT_FALSE(utils::isValidHotkey(hk, true));   // 全局热键必须带主键
}

TEST(HotkeyValidity, MainKeyOnly) {
    Hotkey hk;
    hk.vk = 'A';
    EXPECT_TRUE(utils::isValidHotkey(hk, false));
    EXPECT_TRUE(utils::isValidHotkey(hk, true));
}
