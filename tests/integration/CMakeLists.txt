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

add_executable(spothound_integration_tests
    main.cpp
    test_yandex_integration.cpp
)

add_executable(spothound_acceptance_tests
    main.cpp
    test_acceptance.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/qt/scrapetask.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/qt/yandexscraper.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/qt/yandexcollector.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/qt/yandexparser.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/qt/captchaawarepage.cpp
    ${CMAKE_SOURCE_DIR}/src/adapters/qt/stopwordsstore.cpp
)

target_include_directories(spothound_acceptance_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(spothound_acceptance_tests PRIVATE
    spothound-core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::WebEngineWidgets
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Sql
    GTest::gtest
)

target_include_directories(spothound_integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(spothound_integration_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::WebEngineWidgets
    Qt${QT_VERSION_MAJOR}::Test
    GTest::gtest
)

# Все тесты в одном процессе — SetUpTestSuite загружает страницу один раз.
# gtest_discover_tests запускал бы каждый тест отдельным процессом и триггерил CAPTCHA повторно.
add_test(NAME YandexIntegration
    COMMAND spothound_integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
set_tests_properties(YandexIntegration PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 120
)
