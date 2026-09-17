#pragma once

#include <string>
#include <string_view>
// ReSharper disable once CppUnusedIncludeDirective
#include "resource.h"
#include "lang.h"

class LanguageManager {
public:
    static LanguageManager &instance();

    void setLanguage(Lang lang);

    bool setLanguageByCode(std::string_view code);

    void setLanguageFromSystem();

    [[nodiscard]] Lang currentLang() const noexcept { return m_lang; }

    [[nodiscard]] const char *currentCode() const noexcept;

    [[nodiscard]] std::wstring tr(int id) const;

private:
    LanguageManager() = default;

    Lang m_lang = LANG_DEFAULT;
    int m_baseId = LANG_DEFAULT_BASE;
};

#define TR(id) (LanguageManager::instance().tr(id))
