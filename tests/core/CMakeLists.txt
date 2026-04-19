include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(spothound_core_tests
    test_rules.cpp
    test_stop_words_filter.cpp
    test_place_row_json.cpp
    test_csv_export.cpp
    test_city_registry.cpp
)

target_link_libraries(spothound_core_tests PRIVATE
    spothound-core
    GTest::gtest_main
)

gtest_discover_tests(spothound_core_tests)
