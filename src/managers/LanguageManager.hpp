#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "lang.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "resource.h"

class LanguageManager {
public:
    static LanguageManager &instance();

    void setLanguage(Lang lang);

    bool setLanguageByCode(std::string_view code);

    void setLanguageFromSystem();

    // The language a start without an explicit config code would pick: the system locale mapped
    // to a known code, otherwise LANG_DEFAULT. Pure: it does not change the current language.
    [[nodiscard]] static Lang resolveSystemLanguage();

    [[nodiscard]] Lang currentLang() const noexcept { return m_lang; }

    [[nodiscard]] std::string_view currentCode() const noexcept;

    [[nodiscard]] std::wstring tr(int id) const;

private:
    LanguageManager() = default;

    // Loads the JSON table of the given resource id. A missing or broken table
    // leaves m_table empty, and tr() falls through to the .rc string table.
    void loadTable(int jsonResId);

    Lang m_lang = LANG_DEFAULT;
    int m_baseId = LANG_DEFAULT_BASE;
    std::unordered_map<int, std::wstring> m_table;
    mutable bool m_missingWarned = false;
};

#define TR(id) (LanguageManager::instance().tr(id))
