cmake_minimum_required(VERSION 3.16)
project(elsim VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(ELSIM_BUILD_APP "Build the Qt GUI application" ON)

add_subdirectory(core)
add_subdirectory(plugins)

if(ELSIM_BUILD_APP)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(NOT Qt6_FOUND)
        find_package(Qt5 5.12 COMPONENTS Widgets QUIET)
    endif()
    if(Qt6_FOUND OR Qt5_FOUND)
        add_subdirectory(app)
    else()
        message(WARNING "Qt5/Qt6 Widgets not found - building core, CLI and plugins only. Install qt6-qtbase-devel (or equivalent) to build the GUI.")
    endif()
endif()

enable_testing()
