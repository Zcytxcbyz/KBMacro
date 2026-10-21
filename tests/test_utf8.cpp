// ============================================================
// UTF-8 ↔ 宽字符 转换测试
//
// ToUtf8 / ToWide 是所有"人可见文本"进出 Win32 的关口：
// 日志（spdlog 只收 UTF-8）、JSON（Hotkey 的键名）、文件名。
// 出错时的表现不是崩溃，而是某个字变成 "?" 或字符串被截断，
// 所以这层必须有测试。
//
// 覆盖点：
//   1. 空串：两边都返回空，不走 Win32 调用
//   2. ASCII / 中文 / 非 BMP（emoji）：字节长度与往返
//   3. 内嵌 '\0'：实现显式传长度，不能被当成结尾
//   4. 非法输入：非法 UTF-8 与孤立代理项都替换成 U+FFFD，不崩
//
// 断言统一用"UTF-8 字符串"作为比较形态：宽字符断言失败时
// gtest 打出来的是字节堆，看不出是哪个字。
// ============================================================

#include <gtest/gtest.h>

#include <string>

#include "common/Utils.hpp"

// ---- ToUtf8 ----

TEST(ToUtf8, EmptyStaysEmpty) {
    EXPECT_TRUE(utils::ToUtf8(L"").empty());
    EXPECT_TRUE(utils::ToUtf8(std::wstring_view{}).empty());
}

TEST(ToUtf8, AsciiIsBytePerChar) {
    EXPECT_EQ(utils::ToUtf8(L"KBMacro"), "KBMacro");
}

TEST(ToUtf8, ChineseIsThreeBytesPerChar) {
    // U+4E2D U+6587 → E4 B8 AD / E6 96 87
    const std::string utf8 = utils::ToUtf8(L"中文");
    EXPECT_EQ(utf8.size(), 6u);
    EXPECT_EQ(utf8, std::string("\xE4\xB8\xAD\xE6\x96\x87", 6));
}

TEST(ToUtf8, NonBmpIsFourBytes) {
    // U+1F600 在宽字符里是两个代理项 0xD83D 0xDE00 → F0 9F 98 80
    const std::wstring emoji = L"\xD83D\xDE00";
    const std::string utf8 = utils::ToUtf8(emoji);
    EXPECT_EQ(utf8.size(), 4u);
    EXPECT_EQ(utf8, std::string("\xF0\x9F\x98\x80", 4));
}

TEST(ToUtf8, KeepsEmbeddedNul) {
    // 注意：不能写 ToUtf8(L"a\0b")——字面量隐式转 std::wstring_view 时
    // 按 wcslen 求长度，'\0' 之后的内容根本进不来（这个坑踩过）。
    // 必须显式给长度构造，才能真正把内嵌 NUL 递进函数。
    const std::wstring withNul(L"a\0b", 3);

    const std::string utf8 = utils::ToUtf8(withNul);
    EXPECT_EQ(utf8.size(), 3u);
    EXPECT_EQ(utf8, std::string("a\0b", 3));

    // 回程同样要显式长度 → 往返后内嵌 NUL 还在
    EXPECT_EQ(utils::ToWide(utf8).size(), 3u);
}

TEST(ToUtf8, UnpairedSurrogateBecomesReplacement) {
    // 孤立代理项不是合法 UTF-16：Vista+ 的文档行为是替换成 U+FFFD，
    // 不设 WC_ERR_INVALID_CHARS 时不会失败。
    const std::string utf8 = utils::ToUtf8(L"\xD83D");
    EXPECT_EQ(utf8, std::string("\xEF\xBF\xBD", 3));
}

// ---- ToWide ----

TEST(ToWide, EmptyStaysEmpty) {
    EXPECT_TRUE(utils::ToWide("").empty());
    EXPECT_TRUE(utils::ToWide(std::string_view{}).empty());
}

TEST(ToWide, Ascii) {
    EXPECT_EQ(utils::ToUtf8(utils::ToWide("KBMacro")), "KBMacro");
}

TEST(ToWide, Chinese) {
    const std::wstring wide = utils::ToWide(std::string("\xE4\xB8\xAD\xE6\x96\x87", 6));
    EXPECT_EQ(wide.size(), 2u);
    EXPECT_EQ(utils::ToUtf8(wide), std::string("\xE4\xB8\xAD\xE6\x96\x87", 6));
}

TEST(ToWide, NonBmpBecomesSurrogatePair) {
    const std::wstring wide = utils::ToWide(std::string("\xF0\x9F\x98\x80", 4));
    EXPECT_EQ(wide.size(), 2u);   // 一个字符占两个 wchar_t
    EXPECT_EQ(utils::ToUtf8(wide), std::string("\xF0\x9F\x98\x80", 4));
}

TEST(ToWide, InvalidBytesBecomeReplacement) {
    // 0xFF 在 UTF-8 里不合法 → U+FFFD，不失败、不空串
    const std::wstring wide = utils::ToWide(std::string("\xFF", 1));
    EXPECT_EQ(wide.size(), 1u);
    EXPECT_EQ(utils::ToUtf8(wide), std::string("\xEF\xBF\xBD", 3));
}

// ---- 往返 ----

TEST(Utf8RoundTrip, MixedSamples) {
    constexpr std::wstring_view kSamples[] = {
        L"",
        L"a",
        L"KBMacro 便携键鼠自动化小工具",
        L"标点：，。！？、；：（）《》",
        L"a中b文c",
        L"\xD83D\xDE00",              // 😀
        L"混合 \xD83D\xDE00 与中文",
    };

    for (const std::wstring_view &sample: kSamples) {
        const std::wstring back = utils::ToWide(utils::ToUtf8(sample));
        EXPECT_EQ(utils::ToUtf8(back), utils::ToUtf8(sample))
            << "sample=" << utils::ToUtf8(sample);
    }
}
