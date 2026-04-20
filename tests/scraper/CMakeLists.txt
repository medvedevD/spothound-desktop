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

add_executable(spothound_scraper_tests
    main.cpp
    test_yandex_card.cpp
)

target_compile_definitions(spothound_scraper_tests PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_include_directories(spothound_scraper_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(spothound_scraper_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::WebEngineWidgets
    Qt${QT_VERSION_MAJOR}::Test
    GTest::gtest
)

gtest_discover_tests(spothound_scraper_tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
