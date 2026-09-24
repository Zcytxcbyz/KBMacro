// ============================================================
// ConfigPaths::resolveConfigPath 测试
//
// 这是设计文档里标记为 P1 的测试项。核心逻辑是：
//   - {exePath}.json 存在 → 便携模式
//   - 否则               → APPDATA 模式
//
// 用例在临时目录里操作，不污染真实环境。
// ============================================================

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "common/ConfigPaths.hpp"

namespace {

    // 每个测试用一个唯一的临时目录，测试结束时清掉。
    // 用 ::testing::UnitTest 的当前测试名做后缀，避免并发冲突。
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

        // 在临时目录里造一个假的 exe 路径
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
    // {exe}.json 不存在 → APPDATA 模式
    const auto result = config::resolveConfigPath(fakeExe(), fakeAppData());

    EXPECT_FALSE(result.portable);
    EXPECT_EQ(result.path, fakeAppData() / L"KBMacro" / L"config.json");
}

TEST_F(ConfigPathsTest, PortableFileExistsUsesPortableMode) {
    // 造一个 {exe}.json
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
    // 验证拼接规则：KBMacro.exe → KBMacro.exe.json
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
    // APPDATA 模式下的路径形状：{appData}/KBMacro/config.json
    const auto result = config::resolveConfigPath(fakeExe(), fakeAppData());

    EXPECT_EQ(result.path.parent_path().filename(), L"KBMacro");
    EXPECT_EQ(result.path.filename(), L"config.json");
    EXPECT_EQ(result.path.parent_path().parent_path(), fakeAppData());
}
