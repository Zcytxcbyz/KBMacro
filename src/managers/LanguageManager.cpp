#include "managers/LanguageManager.hpp"

#include <iterator>
#include <windows.h>
#include <wil/resource.h>

#include "common/LangTable.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"

LanguageManager &LanguageManager::instance() {
    static LanguageManager inst;
    return inst;
}

void LanguageManager::loadTable(const int jsonResId) {
    m_table.clear();
    if (jsonResId == 0) {
        return;
    }

    const HINSTANCE instance = ::GetModuleHandleW(nullptr); // NOLINT(*-misplaced-const)
    const HRSRC found = ::FindResourceW(instance, // NOLINT(*-misplaced-const)
        MAKEINTRESOURCEW(jsonResId), reinterpret_cast<LPCWSTR>(RT_RCDATA));
    if (found == nullptr) {
        LOG_WARN("i18n: JSON table resource {} not found, using the fallback table", jsonResId);
        return;
    }

    const HGLOBAL handle = ::LoadResource(instance, found);
    const auto *data = handle != nullptr ? static_cast<const char *>(::LockResource(handle)) : nullptr;
    const DWORD size = ::SizeofResource(instance, found);
    if (data == nullptr || size == 0) {
        LOG_WARN("i18n: JSON table resource {} is empty, using the fallback table", jsonResId);
        return;
    }

    try {
        m_table = parseLangTable(std::string_view(data, size));
        LOG_DEBUG("i18n: loaded {} entr(ies) from JSON resource {}", m_table.size(), jsonResId);
    } catch (const std::exception &ex) {
        // A broken table must not take the UI down: log it and keep the .rc strings.
        LOG_ERROR("i18n: {} (resource {}), using the fallback table", ex.what(), jsonResId);
    }
}

void LanguageManager::setLanguage(const Lang lang) {
    for (const auto &info : kLangs) {
        if (info.id == lang) {
            m_lang = lang;
            m_baseId = info.baseResId;
            m_missingWarned = false;
            loadTable(info.jsonResId);
            return;
        }
    }

    // Unknown value (a corrupted config): fall back to the default language.
    // Assigning the fields directly keeps this from recursing into itself.
    m_lang = LANG_DEFAULT;
    m_baseId = LANG_DEFAULT_BASE;
    m_missingWarned = false;
    loadTable(IDR_LANG_EN_US);
}

// ReSharper disable once CppParameterMayBeConst
bool LanguageManager::setLanguageByCode(std::string_view code) {
    for (const auto &info : kLangs) { // NOLINT(*-use-anyofallof)
        if (code == info.code) {
            m_lang = info.id;
            m_baseId = info.baseResId;
            m_missingWarned = false;
            loadTable(info.jsonResId);
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
    if (!setLanguageByCode(utils::ToUtf8(buf))) {
        setLanguage(LANG_DEFAULT);
    }
}

std::string_view LanguageManager::currentCode() const noexcept {
    for (const auto &info : kLangs) {
        if (info.id == m_lang) return info.code;
    }
    return LANG_DEFAULT_CODE;
}

std::wstring LanguageManager::tr(const int id) const {
    // 1) The language's own JSON table: the primary source at runtime.
    if (const auto it = m_table.find(id); it != m_table.end()) {
        return it->second;
    }

    wchar_t buf[512] = {};

    // 2) The language's fallback string table. Only en-US and zh-CN have one,
    //    and baseResId == 0 marks the languages that do not.
    if (m_baseId != 0) {
        const int len = ::LoadStringW(::GetModuleHandleW(nullptr),
                                      m_baseId + id,
                                      buf,
                                      // ReSharper disable once CppRedundantCastExpression
                                      static_cast<int>(std::size(buf)));
        if (len > 0) return {buf, static_cast<std::size_t>(len)};
    }

    // 3) English last resort, so a missing translation shows English instead of
    //    an empty label.
    if (m_lang != LANG_DEFAULT) {
        const int len = ::LoadStringW(::GetModuleHandleW(nullptr),
                                      LANG_DEFAULT_BASE + id,
                                      buf,
                                      // ReSharper disable once CppRedundantCastExpression
                                      static_cast<int>(std::size(buf)));
        if (len > 0) return {buf, static_cast<std::size_t>(len)};
    }

    // 4) Nothing anywhere: say it once. TR() runs for every label, so this must
    //    not turn into per-call noise.
    if (!m_missingWarned) {
        m_missingWarned = true;
        LOG_WARN("i18n: string id {} is missing in '{}' and in the fallback tables", id, currentCode());
    }
    return {};
}
