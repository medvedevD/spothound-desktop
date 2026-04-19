cmake_minimum_required(VERSION 3.14)

project(spothound VERSION 0.1 LANGUAGES CXX)

add_subdirectory(3rdparty/QXlsx/QXlsx)
add_subdirectory(src/core)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS src/gui)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets WebEngineWidgets Sql WebEngine)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets WebEngineWidgets Sql WebEngine)

set(PROJECT_SOURCES
        src/gui/main.cpp
        src/gui/mainwindow.cpp
        src/gui/mainwindow.h
        src/gui/mainwindow.ui
        src/gui/placesmodel.h
        src/gui/placesmodel.cpp
        src/gui/hintlistwidget.h
        src/adapters/qt/scrapetask.h
        src/adapters/qt/scrapetask.cpp
        src/adapters/qt/yandexscraper.h
        src/adapters/qt/yandexscraper.cpp
        src/adapters/qt/twogisscraper.h
        src/adapters/qt/twogisscraper.cpp
        src/adapters/qt/googlemapsscraper.h
        src/adapters/qt/googlemapsscraper.cpp
        src/adapters/qt/captchaawarepage.h
        src/adapters/qt/captchaawarepage.cpp
        src/adapters/qt/loggingpage.h
        src/adapters/qt/loggingpage.cpp
        src/adapters/qt/stopwordsstore.h
        src/adapters/qt/stopwordsstore.cpp
        resources.qrc
)

if(WIN32)
    list(APPEND PROJECT_SOURCES icons/spothound.rc)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(spothound
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(spothound
        ${PROJECT_SOURCES}
    )
endif()

target_include_directories(spothound PRIVATE src src/gui src/adapters/qt)
target_link_libraries(spothound PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt5::WebEngineWidgets Qt5::WebEngine Qt5::Sql)
target_link_libraries(spothound PRIVATE QXlsx::QXlsx)
target_link_libraries(spothound PRIVATE spothound-core)

set_target_properties(spothound PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(spothound)
endif()

option(BUILD_TESTING "Build unit tests for spothound-core" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests/core)
else()
    add_subdirectory(tests/core EXCLUDE_FROM_ALL)
endif()
