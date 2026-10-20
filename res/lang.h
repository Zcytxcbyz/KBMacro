#pragma once

#define LANG_ZH_CN_BASE  10000
#define LANG_EN_US_BASE  20000

#ifndef RC_INVOKED

#include <iterator>
#include <string_view>

#define LANG_DEFAULT_BASE LANG_ZH_CN_BASE
#define LANG_DEFAULT Lang::ZH_CN
#define LANG_DEFAULT_CODE "zh-CN"

enum class Lang {
    ZH_CN,
    EN_US,
    COUNT
};

struct LangInfo {
    Lang id;
    int baseResId;
    std::string_view code;
    std::wstring_view displayName;
};

inline constexpr LangInfo kLangs[] = {
    {.id = Lang::ZH_CN, .baseResId = LANG_ZH_CN_BASE, .code = "zh-CN", .displayName = L"简体中文"},
    {.id = Lang::EN_US, .baseResId = LANG_EN_US_BASE, .code = "en-US", .displayName = L"English"},
};

static_assert(std::size(kLangs) == static_cast<std::size_t>(Lang::COUNT),
              "The number of kLangs doesn't match the Lang enum");

#endif // RC_INVOKED
