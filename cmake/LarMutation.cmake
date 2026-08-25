include_guard(GLOBAL)

set(LAR_MULL_IR_FRONTEND "" CACHE FILEPATH
    "Mull IR frontend plugin matching the selected Clang compiler")

function(lar_configure_mutation)
    add_library(lar-project-mutation INTERFACE)
    if(NOT LAR_ENABLE_MUTATION)
        return()
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Mutation instrumentation requires Clang/LLVM")
    endif()

    if(NOT LAR_MULL_IR_FRONTEND)
        find_file(lar_mull_ir_frontend
            NAMES
                mull-ir-frontend-19
                mull-ir-frontend-18
                mull-ir-frontend-17
                mull-ir-frontend
            PATHS
                /usr/lib
                /usr/local/lib
                /opt/homebrew/lib
        )
        if(lar_mull_ir_frontend)
            set(LAR_MULL_IR_FRONTEND "${lar_mull_ir_frontend}" CACHE FILEPATH
                "Mull IR frontend plugin matching the selected Clang compiler" FORCE)
        endif()
    endif()
    if(NOT EXISTS "${LAR_MULL_IR_FRONTEND}")
        message(FATAL_ERROR
            "LAR_ENABLE_MUTATION requires LAR_MULL_IR_FRONTEND to name the "
            "Mull plugin matching ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    endif()

    target_compile_options(lar-project-mutation INTERFACE
        "-fpass-plugin=${LAR_MULL_IR_FRONTEND}"
        -g
        -grecord-command-line
        -O0
    )
endfunction()
