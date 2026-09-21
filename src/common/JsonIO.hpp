#pragma once

#include <filesystem>

#include <nlohmann/json.hpp>

// ============================================================
// JSON 文件读写（纯函数）
//
// 从 ConfigPaths 抽出的通用部分，用于"参数导入导出"场景：
// 调用方自己组装/解析 json 对象，本模块只管文件 IO。
//
// 和 ConfigPaths 的区别：
//   - ConfigPaths 带 version 校验、便携/APPDATA 路径判定
//   - JsonIO 只做纯粹的"把 json 写进文件 / 从文件读成 json"
//   - version 包装由调用方决定（参数文件也走 version，但由上层管）
// ============================================================

namespace jsonio {

    // 把 json 写入文件。
    //   - 父目录不存在时自动创建
    //   - 缩进 2 空格，末尾加换行
    //   返回 true  = 成功
    //   返回 false = 失败（详细原因写 LOG_ERROR）
    [[nodiscard]] bool saveTo(const std::filesystem::path &path,
                              const nlohmann::json &j);

    // 从文件读 json。
    //   返回 true  = 成功
    //   返回 false = 失败（文件不存在 / 打开失败 / 解析失败），out 保持默认
    //
    // 内部对失败原因写 LOG_INFO（文件不存在）/ LOG_WARN（打开、解析失败）
    [[nodiscard]] bool loadFrom(const std::filesystem::path &path,
                                nlohmann::json &out);

} // namespace jsonio
