#include "managers/LanguageManager.hpp"

#include <iterator>
#include <windows.h>

LanguageManager& LanguageManager::instance() {
    static LanguageManager inst;
    return inst;
}

void LanguageManager::setLanguage(Lang lang) {
    for (const auto& info : kLangs) {
        if (info.id == lang) {
            m_lang   = lang;
            m_baseId = info.baseResId;
            return;
        }
    }
    m_lang   = LANG_DEFAULT;
    m_baseId = LANG_DEFAULT_BASE;
}

// ReSharper disable once CppParameterMayBeConst
bool LanguageManager::setLanguageByCode(std::wstring_view code) {
    for (const auto& info : kLangs) { // NOLINT(*-use-anyofallof)
        if (code == info.code) {
            m_lang   = info.id;
            m_baseId = info.baseResId;
            return true;
        }
    }
    return false;
}

void LanguageManager::setLanguageFromSystem() {
    wchar_t buf[LOCALE_NAME_MAX_LENGTH] = {};
    if (::GetUserDefaultLocaleName(buf, LOCALE_NAME_MAX_LENGTH) <= 0) {
        setLanguage(LANG_DEFAULT);
        return;
    }
    if (!setLanguageByCode(buf)) {
        setLanguage(LANG_DEFAULT);
    }
}

const wchar_t* LanguageManager::currentCode() const noexcept {
    for (const auto& info : kLangs) {
        if (info.id == m_lang) return info.code;
    }
    return LANG_DEFAULT_CODE;
}

std::wstring LanguageManager::tr(const int id) const {
    wchar_t buf[512] = {};
    const int len = ::LoadStringW(::GetModuleHandleW(nullptr),
                                  m_baseId + id,
                                  buf,
                                  static_cast<int>(std::size(buf)));
    if (len <= 0) return L"";
    return { std::wstring(buf, static_cast<std::size_t>(len)) };
}
