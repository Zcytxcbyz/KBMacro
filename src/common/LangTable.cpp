#include "common/LangTable.hpp"

#include <charconv>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "common/Utils.hpp"

std::unordered_map<int, std::wstring> parseLangTable(const std::string_view utf8Json) {
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(utf8Json.begin(), utf8Json.end());
    } catch (const std::exception &ex) {
        throw std::runtime_error(std::string("not a valid JSON document: ") + ex.what());
    }

    if (!doc.is_object()) {
        throw std::runtime_error("a language table must be a JSON object");
    }

    std::unordered_map<int, std::wstring> table;
    table.reserve(doc.size());

    for (const auto &[key, value] : doc.items()) {
        int id = 0;
        const char *const first = key.data();
        const char *const last = first + key.size();
        const auto [end, error] = std::from_chars(first, last, id);
        if (error != std::errc{} || end != last) {
            throw std::runtime_error("key '" + key + "' is not a numeric string id");
        }
        if (!value.is_string()) {
            throw std::runtime_error("value of '" + key + "' is not a string");
        }
        table[id] = utils::ToWide(value.get_ref<const std::string &>());
    }

    return table;
}
