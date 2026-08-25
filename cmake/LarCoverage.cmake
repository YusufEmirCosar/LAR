include_guard(GLOBAL)

function(lar_configure_coverage)
    add_library(lar-project-coverage INTERFACE)
    if(NOT LAR_ENABLE_COVERAGE)
        return()
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR
            "The coverage preset uses GCC/gcov so one report covers every target")
    endif()
    target_compile_options(lar-project-coverage INTERFACE
        --coverage
        -O0
        -g
    )
    target_link_options(lar-project-coverage INTERFACE --coverage)
endfunction()
