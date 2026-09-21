cmake_minimum_required(VERSION 4.0)
project(KBMacro VERSION 1.0.0 LANGUAGES CXX)

# ============================================================
# 启用 C++20 标准
# ============================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# Cmake 选项
# ============================================================
option(ENABLE_TESTS "Build unit tests" OFF)
option(ENABLE_LOGGING "Enable logging" OFF)

# ============================================================
# 强制 Windows 平台
# ============================================================
if(NOT WIN32)
    message(FATAL_ERROR "KBMacro only supports Windows")
endif()

# ============================================================
# 添加 Windows 特定编译选项
# ============================================================
IF(WIN32)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS") 
ENDIF(WIN32)

# ============================================================
# 查找 Conan 管理的依赖（由 CMakeDeps 生成）
# ============================================================
find_package(wxWidgets REQUIRED COMPONENTS core base adv)
find_package(nlohmann_json REQUIRED)
find_package(wil REQUIRED)
find_package(concurrentqueue REQUIRED)

if(ENABLE_LOGGING)
    find_package(spdlog REQUIRED)
endif()

if(ENABLE_TESTS)
    find_package(GTest REQUIRED)
endif()

# ============================================================
# 收集项目源码（后续再填）
# ============================================================
set(SOURCES
        src/main.cpp
        src/managers/LanguageManager.cpp
        src/common/Log.cpp
        src/common/Utils.cpp
        src/ui/MainFrame.cpp
        src/ui/KBMacroStatusBar.cpp
        src/ui/panels/ClickerPanel.cpp
        src/ui/panels/KeyboardPanel.cpp
        src/ui/panels/RecorderPanel.cpp
        src/ui/panels/SettingsPanel.cpp
        src/ui/panels/AboutPanel.cpp
        src/common/Hotkey.cpp
        src/ui/dialogs/HotkeyCaptureDialog.cpp
        src/ui/ScreenCaptureOverlay.cpp
        src/ui/dialogs/ActionEditDialog.cpp
        src/common/Action.cpp
        src/common/ClickerParams.cpp
        src/common/KeyboardParams.cpp
        src/common/Config.cpp
        src/common/ConfigPaths.cpp
        src/managers/ConfigManager.cpp
        # 在这里添加 .cpp 文件
)

set(HEADERS 
        res/resource.h 
        res/lang.h 
        src/managers/LanguageManager.hpp
        src/common/Log.hpp
        src/common/Utils.hpp
        src/common/Types.hpp
        src/ui/MainFrame.hpp
        src/ui/KBMacroStatusBar.hpp
        src/ui/panels/ClickerPanel.hpp
        src/ui/panels/KeyboardPanel.hpp
        src/ui/panels/RecorderPanel.hpp
        src/ui/panels/SettingsPanel.hpp
        src/ui/panels/AboutPanel.hpp
        src/common/Hotkey.hpp
        src/ui/dialogs/HotkeyCaptureDialog.hpp
        src/ui/ScreenCaptureOverlay.hpp
        src/common/Action.hpp
        src/ui/dialogs/ActionEditDialog.hpp
        src/common/KeyNames.gen.hpp
        src/common/ClickerParams.hpp
        src/common/KeyboardParams.hpp
        src/common/Config.hpp
        src/common/ConfigPaths.hpp
        src/managers/ConfigManager.hpp
        # 在这里添加 .h 文件
)

# ============================================================
# 添加可执行文件
# ============================================================
add_executable(KBMacro WIN32 ${SOURCES} ${HEADERS})

# ============================================================
# 链接 Conan 管理的依赖
# ============================================================
target_link_libraries(KBMacro 
        PRIVATE 
        wxWidgets::wxWidgets 
        nlohmann_json::nlohmann_json 
        WIL::WIL 
        concurrentqueue::concurrentqueue
)

# ============================================================
# 日志模块（可选）
# ============================================================
if(ENABLE_LOGGING)
    target_link_libraries(KBMacro PRIVATE spdlog::spdlog)
    target_compile_definitions(KBMacro PRIVATE ENABLE_LOGGING)
endif()

# ============================================================
# Windows 资源文件（可选）
# ============================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/resource.rc")
    target_sources(KBMacro PRIVATE res/resource.rc)
endif()

# ============================================================
# 包含目录（后续再填）
# ============================================================
target_include_directories(KBMacro 
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src 
        ${CMAKE_CURRENT_SOURCE_DIR}/res 
        # 在这里添加包含路径
)

# ============================================================
# 预处理器宏
# ============================================================
target_compile_definitions(KBMacro PRIVATE 
        _CRT_SECURE_NO_WARNINGS 
        _USE_MATH_DEFINES 
        NOMINMAX 
        WIN32_LEAN_AND_MEAN
        KBMACRO_VERSION="${PROJECT_VERSION}"
        KBMACRO_HOMEPAGE="https://example.com/kbmacro"
)

# ============================================================
# 修复 MSVC + Ninja 依赖扫描模式下缺少目标架构宏的问题
# （cl.exe -scanDependencies 不注入 _AMD64_ / _X86_，
#   winnt.h 会 #error "No Target Architecture"）
# ============================================================
if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(KBMacro PRIVATE _AMD64_)
    else()
        target_compile_definitions(KBMacro PRIVATE _X86_)
    endif()
endif()

# ============================================================
# 编译选项
# ============================================================
target_compile_options(KBMacro PRIVATE /utf-8)

# ============================================================
# 输出文件名和目录（单 EXE）
# ============================================================
set_target_properties(KBMacro PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
        OUTPUT_NAME "KBMacro"
)

# ============================================================
# 静态链接 CRT 已由 Conan toolchain 控制，无需手动设置
# ============================================================

# ============================================================
# 单元测试（可选）
# ============================================================
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "KBMacro configured successfully")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")