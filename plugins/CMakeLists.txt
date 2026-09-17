set(ELSIM_PLUGIN_DIR ${CMAKE_BINARY_DIR}/plugins CACHE PATH "plugin output dir")
file(MAKE_DIRECTORY ${ELSIM_PLUGIN_DIR})

function(elsim_plugin name)
    add_library(${name} MODULE ${name}.c)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/core/include)
    target_compile_definitions(${name} PRIVATE EC_PLUGIN_BUILD)
    target_link_libraries(${name} PRIVATE elsim-core)
    set_target_properties(${name} PROPERTIES PREFIX "" FOLDER plugins)
    set_target_properties(${name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${ELSIM_PLUGIN_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${ELSIM_PLUGIN_DIR})
endfunction()

elsim_plugin(led)
elsim_plugin(switch)
