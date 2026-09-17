find_package(Qt6 COMPONENTS Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 COMPONENTS Widgets QUIET)
endif()
if(NOT Qt6_FOUND AND NOT (TARGET Qt5::Widgets))
    message(FATAL_ERROR "Qt5 (>=5.15) or Qt6 Widgets is required to build the GUI")
endif()

if(Qt6_FOUND)
    set(QT_LIBRARIES Qt6::Widgets)
else()
    set(QT_LIBRARIES Qt5::Widgets)
endif()

set(CMAKE_AUTOMOC ON)

add_executable(elsim-gui
    src/main.cpp
    src/mainwindow.h src/mainwindow.cpp
    src/schematicscene.h src/schematicscene.cpp
    src/componentitem.h src/componentitem.cpp
    src/wireitem.h src/wireitem.cpp
    src/netlistcompiler.h src/netlistcompiler.cpp
    src/plotpane.h src/plotpane.cpp
    src/symbols.h src/symbols.cpp
    src/simworker.h src/simworker.cpp
)
target_include_directories(elsim-gui PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(elsim-gui PRIVATE elsim-core ${QT_LIBRARIES})
# place the GUI next to the plugins/ folder so external components load
set_target_properties(elsim-gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(WIN32)
    set_target_properties(elsim-gui PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS elsim-gui RUNTIME DESTINATION bin)
