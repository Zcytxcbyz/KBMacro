# ============================================================
# KBMacro unit tests
#
# Pulled in by the root CMakeLists through add_subdirectory(tests).
# Tested sources are listed explicitly instead of being extracted into a library:
# the current subjects (serialisation, enums, paths) do not depend on wxWidgets
# and build standalone; extract KBMacroCore once more modules need it.
# ============================================================

include(GoogleTest)

# Tested source files (add or remove as needed; overlaps the root SOURCES list)
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
        # clicker module: needs input::click / wheel / moveTo from win32/Input.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/Clicker.cpp
        ${CMAKE_SOURCE_DIR}/src/win32/Input.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/Keyboard.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/Playback.cpp
        ${CMAKE_SOURCE_DIR}/src/common/CoordAdapt.cpp
        ${CMAKE_SOURCE_DIR}/src/modules/RecorderNormalize.cpp
        ${CMAKE_SOURCE_DIR}/src/common/ActionScale.cpp
)

# Test files
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
        test_recorder_normalize.cpp
        test_ratelimiter.cpp
        test_actionscale.cpp
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

# Same compile settings as the main target
target_compile_definitions(KBMacroTests
        PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        # Required: build win32/Input.cpp in dry-run mode inside the tests,
        # otherwise they would inject real mouse events into the desktop.
        KBMACRO_DRY_RUN
)

# Same MSVC target-architecture workaround as the main target (winnt.h needs it)
if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(KBMacroTests PRIVATE _AMD64_)
    else()
        target_compile_definitions(KBMacroTests PRIVATE _X86_)
    endif()
endif()

target_compile_options(KBMacroTests PRIVATE /utf-8)

gtest_discover_tests(KBMacroTests)
