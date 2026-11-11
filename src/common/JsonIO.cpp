#include "common/JsonIO.hpp"

#include <fstream>

#include "common/Log.hpp"
#include "common/Utils.hpp"

namespace jsonio {

    bool saveTo(const std::filesystem::path &path, const nlohmann::json &j) {
        const std::string pathStr = utils::ToUtf8(path.wstring());

        // create the parent directory when missing
        if (const auto parent = path.parent_path(); !parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                LOG_ERROR("jsonio: cannot create directory: {} ({})",
                          utils::ToUtf8(parent.wstring()), ec.message());
                return false;
            }
        }

        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            LOG_ERROR("jsonio: cannot open file for write: {}", pathStr);
            return false;
        }

        try {
            ofs << j.dump(2) << '\n';
        } catch (const std::exception &e) {
            LOG_ERROR("jsonio: write failed: {} ({})", pathStr, e.what());
            return false;
        }

        if (!ofs) {
            LOG_ERROR("jsonio: write stream error: {}", pathStr);
            return false;
        }

        LOG_DEBUG("jsonio: saved to {}", pathStr);
        return true;
    }

    bool loadFrom(const std::filesystem::path &path, nlohmann::json &out) {
        const std::string pathStr = utils::ToUtf8(path.wstring());

        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) {
            LOG_INFO("jsonio: file not found: {}", pathStr);
            return false;
        }

        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) {
            LOG_WARN("jsonio: cannot open file: {}", pathStr);
            return false;
        }

        try {
            ifs >> out;
        } catch (const std::exception &e) {
            LOG_WARN("jsonio: parse failed: {} ({})", pathStr, e.what());
            return false;
        }

        LOG_DEBUG("jsonio: loaded from {}", pathStr);
        return true;
    }

} // namespace jsonio
