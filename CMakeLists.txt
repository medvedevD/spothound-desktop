cmake_minimum_required(VERSION 3.5)

project(spothound VERSION 0.1 LANGUAGES CXX)

add_subdirectory(3rdparty/QXlsx/QXlsx)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets WebEngineWidgets Sql WebEngine)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets WebEngineWidgets Sql WebEngine)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        scrapetask.h
        scrapetask.cpp
        rules.h
        rules.cpp
        yandexscraper.h
        yandexscraper.cpp
        placerow.h
        placesmodel.h
        placesmodel.cpp
        loggingpage.h
        loggingpage.cpp
        captchaawarepage.h
        captchaawarepage.cpp
        stopwordsstore.h
        stopwordsstore.cpp
        stopwordsdialog.h
        stopwordsdialog.cpp
        stopwordsdialog.ui
        cityregistry.h
        cityregistry.cpp
        twogisscraper.h
        twogisscraper.cpp
        googlemapsscraper.h
        googlemapsscraper.cpp
        resources.qrc
)

if(WIN32)
    list(APPEND PROJECT_SOURCES spothound.rc)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(spothound
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET spothound APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(spothound SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(spothound
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(spothound PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt5::WebEngineWidgets Qt5::WebEngine Qt5::Sql)
target_link_libraries(spothound PRIVATE QXlsx::QXlsx)

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
