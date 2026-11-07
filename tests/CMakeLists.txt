# ============================================================
# KBMacro 单元测试
#
# 由根 CMakeLists 通过 add_subdirectory(tests) 引入。
# 被测源码直接列出，不抽库——理由：目前被测的几块（序列化、
# 枚举、路径判定）不依赖 wxWidgets，独立可编；等 T2 引入更多
# 模块、依赖变复杂时再考虑抽 KBMacroCore。
# ============================================================

include(GoogleTest)

# 被测源文件（按需增删，与根 CMakeLists 的 SOURCES 有重叠部分）
set(TESTED_SOURCES
        ${CMAKE_SOURCE_DIR}/src/common/Action.cpp
        ${CMAKE_SOURCE_DIR}/src/common/ClickerParams.cpp
        ${CMAKE_SOURCE_DIR}/src/common/KeyboardParams.cpp
        ${CMAKE_SOURCE_DIR}/src/common/Config.cpp
        ${CMAKE_SOURCE_DIR}/src/common/ConfigPaths.cpp
        ${CMAKE_SOURCE_DIR}/src/common/Hotkey.cpp
        ${CMAKE_SOURCE_DIR}/src/common/Utils.cpp
        ${CMAKE_SOURCE_DIR}/src/common/Timer.cpp
        ${CMAKE_SOURCE_DIR}/src/common/RunController.cpp
        # 连点器模块：依赖 win32/Input.cpp 里的 input::click/wheel/moveTo
        ${CMAKE_SOURCE_DIR}/src/modules/Clicker.cpp
        ${CMAKE_SOURCE_DIR}/src/win32/Input.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/Keyboard.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/Playback.cpp
        ${CMAKE_SOURCE_DIR}/src/common/CoordAdapt.cpp
)

# 测试文件
set(TEST_SOURCES
        test_serialization.cpp
        test_enums.cpp
        test_configpaths.cpp
        test_timer.cpp
        test_runcontroller.cpp
        test_hotkey_text.cpp
        test_utf8.cpp
        test_clicker.cpp
        test_keyboard.cpp
        test_playback.cpp
        test_coordadapt.cpp
)

add_executable(KBMacroTests ${TEST_SOURCES} ${TESTED_SOURCES})

target_link_libraries(KBMacroTests
        PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
        WIL::WIL
)

target_include_directories(KBMacroTests
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/res
)

# 与主目标一致的编译设置
target_compile_definitions(KBMacroTests
        PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        # 必须：让 win32/Input.cpp 在测试里编译成干跑版本，
        # 否则单测会真的往当前桌面发鼠标事件。
        KBMACRO_DRY_RUN
)

# 与主目标一致的 MSVC 架构宏补丁（winnt.h 需要）
if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(KBMacroTests PRIVATE _AMD64_)
    else()
        target_compile_definitions(KBMacroTests PRIVATE _X86_)
    endif()
endif()

target_compile_options(KBMacroTests PRIVATE /utf-8)

gtest_discover_tests(KBMacroTests)
