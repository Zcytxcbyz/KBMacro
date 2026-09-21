#pragma once

#include <filesystem>

#include "common/Config.hpp"

// ============================================================
// 配置文件的路径判定与读写（纯函数）
//
// 从 ConfigManager 抽出来的原因：
//   1. 路径判定算法要单测（P1），单例类不好注入假路径
//   2. IO 逻辑和生命周期管理（load 一次、setter 即写）是两件事
//
// 纯函数无全局状态，参数全部显式传入，测试可直接喂假路径。
// 真实路径由 ConfigManager 提供。
// ============================================================

namespace config {

    // 配置路径判定结果
    struct ResolvedPath {
        std::filesystem::path path;
        bool portable = false;  // true = 便携模式（EXE 同目录 {exe}.json）
    };

    // 判定配置路径（纯函数，无 IO）
    //   如果 {exePath}.json 已存在 → 便携模式，用该文件
    //   否则                     → APPDATA 模式，
    //                              {appDataDir}/KBMacro/config.json
    //
    // 注意：portable 的判定依据是"文件是否存在"，而不是"是否可写"。
    // 存在但不可写的场景属于用户手动设置的错误，此时 load/save 会失败，
    // 由调用方决定后续行为。
    [[nodiscard]] ResolvedPath resolveConfigPath(
        const std::filesystem::path &exePath,
        const std::filesystem::path &appDataDir);

    // 从文件读取配置。
    //   返回 true  = 成功加载
    //   返回 false = 未加载（以下任一：文件不存在 / 解析失败 / 版本过高），
    //                out 保持默认值
    //
    // 内部对失败原因写 LOG_INFO（文件不存在）/ LOG_WARN（解析失败、版本过高）。
    [[nodiscard]] bool loadFrom(const std::filesystem::path &path, Config &out);

    // 将配置写入文件。父目录不存在时自动创建。
    //   返回 true  = 写入成功
    //   返回 false = 写入失败（详细原因写 LOG_ERROR）
    [[nodiscard]] bool saveTo(const std::filesystem::path &path, const Config &cfg);

} // namespace config
