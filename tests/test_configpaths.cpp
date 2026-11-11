// ============================================================
// ConfigPaths::resolveConfigPath tests
//
// This is the P1 test item from the design doc. The core logic is:
//   - {exePath}.json exists → portable mode
//   - otherwise         → APPDATA mode
//
// Cases work in a temp directory and do not pollute the real environment.
// ============================================================

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "common/ConfigPaths.hpp"

namespace {

    // each test uses a unique temp directory, removed on teardown.
    // suffix with the current ::testing::UnitTest name to avoid concurrent clashes.
    class ConfigPathsTest : public ::testing::Test {
    protected:
        void SetUp() override {
            const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
            m_dir = std::filesystem::temp_directory_path()
                    / ("kbmacro_test_" + std::string(info->name()));
            std::error_code ec;
            std::filesystem::remove_all(m_dir, ec);
            std::filesystem::create_directories(m_dir, ec);
            ASSERT_FALSE(ec) << "cannot create temp dir: " << ec.message();
        }

        void TearDown() override {
            std::error_code ec;
            std::filesystem::remove_all(m_dir, ec);
        }

        // build a fake exe path inside the temp directory
        [[nodiscard]] std::filesystem::path fakeExe() const {
            return m_dir / L"KBMacro.exe";
        }

        [[nodiscard]] std::filesystem::path fakeAppData() const {
            return m_dir / L"AppData";
        }

        std::filesystem::path m_dir;
    };

} // namespace

TEST_F(ConfigPathsTest, NoPortableFileFallsBackToAppData) {
    // {exe}.json absent → APPDATA mode
    const auto result = config::resolveConfigPath(fakeExe(), fakeAppData());

    EXPECT_FALSE(result.portable);
    EXPECT_EQ(result.path, fakeAppData() / L"KBMacro" / L"config.json");
}

TEST_F(ConfigPathsTest, PortableFileExistsUsesPortableMode) {
    // create a {exe}.json
    auto portablePath = fakeExe();
    portablePath.concat(L".json");
    {
        std::ofstream ofs(portablePath);
        ofs << "{}";
    }

    const auto result = config::resolveConfigPath(fakeExe(), fakeAppData());

    EXPECT_TRUE(result.portable);
    EXPECT_EQ(result.path, portablePath);
}

TEST_F(ConfigPathsTest, PortablePathIsExePathPlusJson) {
    // check the append rule: KBMacro.exe → KBMacro.exe.json
    auto portablePath = fakeExe();
    portablePath.concat(L".json");
    {
        std::ofstream ofs(portablePath);
        ofs << "{}";
    }

    const auto result = config::resolveConfigPath(fakeExe(), fakeAppData());

    EXPECT_EQ(result.path.filename(), L"KBMacro.exe.json");
    EXPECT_EQ(result.path.parent_path(), fakeExe().parent_path());
}

TEST_F(ConfigPathsTest, AppDataPathStructure) {
    // path shape in APPDATA mode: {appData}/KBMacro/config.json
    const auto result = config::resolveConfigPath(fakeExe(), fakeAppData());

    EXPECT_EQ(result.path.parent_path().filename(), L"KBMacro");
    EXPECT_EQ(result.path.filename(), L"config.json");
    EXPECT_EQ(result.path.parent_path().parent_path(), fakeAppData());
}
