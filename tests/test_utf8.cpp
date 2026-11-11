// ============================================================
// UTF-8 ↔ wide character conversion tests
//
// ToUtf8 / ToWide are the gate for all "human-visible text" entering and leaving Win32:
// logs (spdlog only takes UTF-8), JSON (Hotkey key names), file names.
// the symptom is not a crash but one character turning into "?" or a truncated string,
// so this layer must be tested.
//
// coverage points:
//   1. empty string: both sides return empty, no Win32 call is made
//   2. ASCII / Chinese / non-BMP (emoji): byte length and round-trip
//   3. embedded '\0': the implementation takes the length explicitly, it must not end the string
//   4. invalid input: invalid UTF-8 and lone surrogates both become U+FFFD, no crash
//
// assertions always compare in UTF-8 form: a failed wide-character assertion
// makes gtest print a byte pile, where the offending character is unrecognizable.
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
    // U+1F600 in a wide string is two surrogates 0xD83D 0xDE00 → F0 9F 98 80
    const std::wstring emoji = L"\xD83D\xDE00";
    const std::string utf8 = utils::ToUtf8(emoji);
    EXPECT_EQ(utf8.size(), 4u);
    EXPECT_EQ(utf8, std::string("\xF0\x9F\x98\x80", 4));
}

TEST(ToUtf8, KeepsEmbeddedNul) {
    // note: do not write ToUtf8(L"a\0b"): implicit literal to std::wstring_view conversion
    // measures length with wcslen, so anything after '\0' never gets in (we hit this).
    // construct with an explicit length to actually pass the embedded NUL through.
    const std::wstring withNul(L"a\0b", 3);

    const std::string utf8 = utils::ToUtf8(withNul);
    EXPECT_EQ(utf8.size(), 3u);
    EXPECT_EQ(utf8, std::string("a\0b", 3));

    // the way back also needs an explicit length → the embedded NUL survives the round-trip
    EXPECT_EQ(utils::ToWide(utf8).size(), 3u);
}

TEST(ToUtf8, UnpairedSurrogateBecomesReplacement) {
    // a lone surrogate is not valid UTF-16: the documented Vista+ behavior is to replace it with U+FFFD,
    // and it does not fail without WC_ERR_INVALID_CHARS.
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
    EXPECT_EQ(wide.size(), 2u);   // one character takes two wchar_t
    EXPECT_EQ(utils::ToUtf8(wide), std::string("\xF0\x9F\x98\x80", 4));
}

TEST(ToWide, InvalidBytesBecomeReplacement) {
    // 0xFF is invalid in UTF-8 → U+FFFD, no failure, no empty string
    const std::wstring wide = utils::ToWide(std::string("\xFF", 1));
    EXPECT_EQ(wide.size(), 1u);
    EXPECT_EQ(utils::ToUtf8(wide), std::string("\xEF\xBF\xBD", 3));
}

// ---- round-trip ----

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
