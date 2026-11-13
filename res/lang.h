#pragma once

// Base ids of the fallback string tables (LoadStringW). Only the languages that
// still ship a .rc table need one; every language uses its JSON table first.
// The step is wide enough for base + 10048 to stay inside 16 bits, so at most
// five string-table languages (50000 + 10048 = 60048).
#define LANG_EN_US_BASE  10000
#define LANG_ZH_CN_BASE  20000

#ifndef RC_INVOKED

#include <iterator>
#include <string_view>

#include "resource.h"   // IDR_LANG_*: the primary JSON tables

#define LANG_DEFAULT_BASE LANG_EN_US_BASE
#define LANG_DEFAULT Lang::EN_US
#define LANG_DEFAULT_CODE "en-US"

enum class Lang {
    EN_US,
    ZH_CN,
    DE_DE,
    COUNT
};

// One entry per language.
//   jsonResId : RCDATA id of the JSON table, the primary source at runtime
//   baseResId : base id of the fallback string table from res/langs/*.rc,
//               0 = this language has no .rc table
//
// The two ids carry the whole story, so there is no separate "source" enum:
// a single field cannot contradict the other one.
struct LangInfo {
    Lang id;
    int jsonResId;
    int baseResId;
    std::string_view code;
    std::wstring_view displayName;
};

inline constexpr LangInfo kLangs[] = {
    {
        .id = Lang::EN_US,
        .jsonResId = IDR_LANG_EN_US,
        .baseResId = LANG_EN_US_BASE,
        .code = "en-US",
        .displayName = L"English"
    },
    {
        .id = Lang::ZH_CN,
        .jsonResId = IDR_LANG_ZH_CN,
        .baseResId = LANG_ZH_CN_BASE,
        .code = "zh-CN",
        .displayName = L"简体中文"
    },
    {
        .id = Lang::DE_DE,
        .jsonResId = IDR_LANG_DE_DE,
        .baseResId = 0, // no .rc fallback table: JSON or English only
        .code = "de-DE",
        .displayName = L"Deutsch"
    },
};

static_assert(std::size(kLangs) == static_cast<std::size_t>(Lang::COUNT),
              "The number of kLangs doesn't match the Lang enum");

#endif // RC_INVOKED
