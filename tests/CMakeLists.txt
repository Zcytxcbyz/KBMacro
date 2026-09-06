find_package(GTest REQUIRED)

add_executable(kbmacro_tests
    test_clicker.cpp
    test_keyboard.cpp
    test_macro_serialization.cpp
)

target_link_libraries(kbmacro_tests
    PRIVATE
    GTest::gtest_main
    KBMacro
)

add_test(NAME kbmacro_tests COMMAND kbmacro_tests)
