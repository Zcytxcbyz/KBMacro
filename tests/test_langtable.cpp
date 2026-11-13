// ============================================================
// LangTable: JSON language table -> id map
//
// Pure function only, no resources and no Windows API. Covers: a normal table,
// escapes and non-ASCII, a non-object document, bad keys, non-string values.
// ============================================================
#include <stdexcept>

#include <gtest/gtest.h>

#include "common/LangTable.hpp"

TEST(LangTable, ParsesNumericKeys) {
    const auto table = parseLangTable(R"({"1001": "KBMacro", "5001": "Interval (ms):"})");

    ASSERT_EQ(table.size(), 2U);
    EXPECT_EQ(table.at(1001), L"KBMacro");
    EXPECT_EQ(table.at(5001), L"Interval (ms):");
}

TEST(LangTable, DecodesEscapesAndNonAscii) {
    const auto table = parseLangTable(R"({"1230": "line1\nline2", "2000": "中文"})");

    ASSERT_EQ(table.size(), 2U);
    EXPECT_EQ(table.at(1230), L"line1\nline2");
    EXPECT_EQ(table.at(2000), L"中文");
}

TEST(LangTable, RejectsMalformedDocument) {
    EXPECT_THROW(static_cast<void>(parseLangTable(R"({"1": )")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(parseLangTable("[]")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(parseLangTable("42")), std::runtime_error);
}

TEST(LangTable, RejectsBadKeysAndValues) {
    EXPECT_THROW(static_cast<void>(parseLangTable(R"({"abc": "x"})")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(parseLangTable(R"({"1x": "x"})")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(parseLangTable(R"({"1001": 42})")), std::runtime_error);
}
