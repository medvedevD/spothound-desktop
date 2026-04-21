include(FetchContent)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

add_library(spothound-core STATIC
    rules.cpp
    stop_words_filter.cpp
    csv_export.cpp
    city_registry.cpp
    geo_grid.cpp
    place_row.h
    place_row_json.h
    rules.h
    stop_words_filter.h
    csv_export.h
    city_registry.h
    geo_grid.h
    scraper_events.h
    i_scraper.h
    scrape_stats.h
)
target_include_directories(spothound-core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/..   # src/ — consumers use "core/rules.h"
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}      # src/core/ — internal includes use "rules.h"
)
target_compile_features(spothound-core PUBLIC cxx_std_17)
target_link_libraries(spothound-core PUBLIC nlohmann_json::nlohmann_json)
# No Qt linkage — any #include <Qt*> in a core file will break the build

option(ENABLE_COVERAGE "Enable gcov coverage instrumentation (GCC/Clang only)" OFF)
if(ENABLE_COVERAGE AND NOT MSVC)
    target_compile_options(spothound-core PUBLIC --coverage -O0 -g)
    target_link_options(spothound-core PUBLIC --coverage)
endif()
