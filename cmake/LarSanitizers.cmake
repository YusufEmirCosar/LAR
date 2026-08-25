include_guard(GLOBAL)

function(lar_configure_sanitizers)
    add_library(lar-project-sanitizers INTERFACE)

    if(LAR_ENABLE_TSAN AND (LAR_ENABLE_ASAN OR LAR_ENABLE_UBSAN))
        message(FATAL_ERROR
            "ThreadSanitizer cannot be combined with ASan/UBSan in this build")
    endif()
    if(NOT LAR_ENABLE_ASAN AND NOT LAR_ENABLE_UBSAN AND NOT LAR_ENABLE_TSAN)
        return()
    endif()
    if(MSVC)
        message(FATAL_ERROR "The selected sanitizer preset requires GCC or Clang")
    endif()

    set(sanitizers "")
    if(LAR_ENABLE_ASAN)
        list(APPEND sanitizers address)
    endif()
    if(LAR_ENABLE_UBSAN)
        list(APPEND sanitizers undefined)
    endif()
    if(LAR_ENABLE_TSAN)
        list(APPEND sanitizers thread)
    endif()
    list(JOIN sanitizers "," sanitizer_list)

    target_compile_options(lar-project-sanitizers INTERFACE
        "-fsanitize=${sanitizer_list}"
        -fno-omit-frame-pointer
    )
    target_link_options(lar-project-sanitizers INTERFACE
        "-fsanitize=${sanitizer_list}"
    )
endfunction()
