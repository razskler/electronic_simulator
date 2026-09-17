add_library(elsim-core SHARED
    src/matrix.c
    src/sys.c
    src/netlist.c
    src/registry.c
    src/builtin.c
    src/results.c
    src/solver_dc.c
    src/solver_ac.c
    src/solver_tr.c
)
target_include_directories(elsim-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(elsim-core PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(elsim-core PRIVATE -Wall -Wextra)
target_link_libraries(elsim-core PRIVATE m)
if(UNIX)
    target_link_libraries(elsim-core PRIVATE dl)
endif()

add_executable(elsim cli/main.c)
target_link_libraries(elsim PRIVATE elsim-core m)
if(UNIX)
    target_link_libraries(elsim PRIVATE dl)
endif()

add_executable(elsim_tests tests/test_all.c)
target_link_libraries(elsim_tests PRIVATE elsim-core m)
add_test(NAME core_tests COMMAND elsim_tests)
