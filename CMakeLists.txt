cmake_minimum_required(VERSION 4.0)
project(KBMacro VERSION 1.0.0 LANGUAGES CXX)

# ============================================================
# 1. 启用 C++20 标准
# ============================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# 2. 强制 Windows 平台
# ============================================================
if(NOT WIN32)
    message(FATAL_ERROR "KBMacro only supports Windows")
endif()

# ============================================================
# 3. 查找 Conan 管理的依赖（由 CMakeDeps 生成）
# ============================================================
find_package(wxWidgets)
find_package(nlohmann_json)
find_package(GTest) # (可选)

# ============================================================
# 4. 收集项目源码（后续再填）
# ============================================================
set(SOURCES
    src/main.cpp
    # 在这里添加 .cpp 文件
)

set(HEADERS
    # 在这里添加 .h 文件
)

# ============================================================
# 5. 添加可执行文件
# ============================================================
add_executable(KBMacro WIN32 ${SOURCES} ${HEADERS})

# ============================================================
# 6. 链接 Conan 管理的依赖
# ============================================================
target_link_libraries(KBMacro
    PRIVATE
    wxWidgets::wxWidgets
    nlohmann_json::nlohmann_json
)

# ============================================================
# 7. Windows 资源文件（可选）
# ============================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/resource.rc")
    target_sources(KBMacro PRIVATE res/resource.rc)
endif()

# ============================================================
# 8. 包含目录（后续再填）
# ============================================================
target_include_directories(KBMacro
    PRIVATE
    # 在这里添加包含路径
)

# ============================================================
# 9. 预处理器宏
# ============================================================
target_compile_definitions(KBMacro PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _USE_MATH_DEFINES
)

# ============================================================
# 10. 编译选项
# ==========
target_compile_options(KBMacro PRIVATE /utf-8)

# ============================================================
# 11. 输出文件名和目录（单 EXE）
# ============================================================
set_target_properties(KBMacro PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "KBMacro"
)

# ============================================================
# 12. 静态链接 CRT 已由 Conan toolchain 控制，无需手动设置
# ============================================================

# ============================================================
# 12. 单元测试（可选）
# ============================================================
option(BUILD_TESTING "Build unit tests" OFF)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "KBMacro configured successfully")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")