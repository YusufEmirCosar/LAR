include_guard(GLOBAL)

include(CheckCXXCompilerFlag)

function(lar_configure_hardening)
    add_library(lar-project-hardening INTERFACE)
    if(NOT LAR_ENABLE_HARDENING)
        return()
    endif()

    set(CMAKE_POSITION_INDEPENDENT_CODE ON PARENT_SCOPE)
    if(MSVC)
        target_compile_options(lar-project-hardening INTERFACE /sdl /guard:cf)
        target_link_options(lar-project-hardening INTERFACE /guard:cf)
        return()
    endif()

    check_cxx_compiler_flag("-fstack-protector-strong" LAR_HAS_STACK_PROTECTOR)
    if(LAR_HAS_STACK_PROTECTOR)
        target_compile_options(lar-project-hardening INTERFACE
            -fstack-protector-strong
        )
    endif()
    if(UNIX AND NOT APPLE)
        target_compile_definitions(lar-project-hardening INTERFACE
            $<$<CONFIG:Release>:_FORTIFY_SOURCE=2>
        )
        target_link_options(lar-project-hardening INTERFACE
            "LINKER:-z,defs"
            "LINKER:-z,noexecstack"
        )
    endif()
endfunction()
