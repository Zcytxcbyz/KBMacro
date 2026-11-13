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
    ZH_TW,
    DE_DE,
    FR_FR,
    JA_JP,
    IT_IT,
    ES_ES,
    RU_RU,
    COUNT
}; // enum class Lang

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
    }, // Lang::EN_US
    {
        .id = Lang::ZH_CN,
        .jsonResId = IDR_LANG_ZH_CN,
        .baseResId = LANG_ZH_CN_BASE,
        .code = "zh-CN",
        .displayName = L"简体中文"
    }, // Lang::ZH_CN
    {
        .id = Lang::ZH_TW,
        .jsonResId = IDR_LANG_ZH_TW,
        .baseResId = 0, // no .rc fallback table: JSON or English only
        .code = "zh-TW",
        .displayName = L"繁體中文"
    }, // Lang::ZH_TW
    {
        .id = Lang::DE_DE,
        .jsonResId = IDR_LANG_DE_DE,
        .baseResId = 0, // no .rc fallback table: JSON or English only
        .code = "de-DE",
        .displayName = L"Deutsch"
    }, // Lang::de_DE
    {
        .id = Lang::FR_FR,
        .jsonResId = IDR_LANG_FR_FR,
        .baseResId = 0, // no .rc fallback table: JSON or English only
        .code = "fr-FR",
        .displayName = L"Français"
    }, // Lang::FR_FR
    {
        .id = Lang::JA_JP,
        .jsonResId = IDR_LANG_JA_JP,
        .baseResId = 0, // no .rc fallback table: JSON or English only
        .code = "ja-JP",
        .displayName = L"日本語"
    }, // Lang::JA_JP
    {
        .id = Lang::IT_IT,
        .jsonResId = IDR_LANG_IT_IT,
        .baseResId = 0, // no .rc fallback table: JSON or English only
        .code = "it-IT",
        .displayName = L"Italiano"
    }, // Lang::IT_IT
    {
        .id = Lang::ES_ES,
        .jsonResId = IDR_LANG_ES_ES,
        .baseResId = 0, // no .rc fallback table: JSON or English only
        .code = "es-ES",
        .displayName = L"Español"
    }, // Lang::ES_ES
    {
        .id = Lang::RU_RU,
        .jsonResId = IDR_LANG_RU_RU,
        .baseResId = 0, // no .rc fallback table: JSON or English only
        .code = "ru-RU",
        .displayName = L"Русский"
    }, // Lang::RU_RU
};

static_assert(std::size(kLangs) == static_cast<std::size_t>(Lang::COUNT),
              "The number of kLangs doesn't match the Lang enum");

#endif // RC_INVOKED
