include_guard(GLOBAL)

include(CheckCXXSourceCompiles)

function(lar_detect_libfuzzer)
    if(NOT LAR_BUILD_FUZZERS)
        set(LAR_HAS_LIBFUZZER OFF PARENT_SCOPE)
        return()
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "LAR_BUILD_FUZZERS requires a Clang-family compiler")
    endif()

    set(saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fsanitize=fuzzer")
    check_cxx_source_compiles(
        "#include <cstddef>
         #include <cstdint>
         extern \"C\" int LLVMFuzzerTestOneInput(
             const std::uint8_t *, std::size_t) { return 0; }"
        detected_libfuzzer
    )
    set(CMAKE_REQUIRED_FLAGS "${saved_required_flags}")
    set(LAR_HAS_LIBFUZZER ${detected_libfuzzer} PARENT_SCOPE)
endfunction()

function(lar_configure_fuzzer target)
    if(LAR_HAS_LIBFUZZER)
        target_compile_options(${target} PRIVATE
            -fsanitize=fuzzer,address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
            -fsanitize=fuzzer,address,undefined
        )
    else()
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
    endif()
endfunction()

function(lar_instrument_fuzzed_library target)
    if(LAR_HAS_LIBFUZZER)
        set(compile_sanitizers fuzzer-no-link,address,undefined)
    else()
        set(compile_sanitizers address,undefined)
    endif()
    target_compile_options(${target} PRIVATE
        "-fsanitize=${compile_sanitizers}"
        -fno-omit-frame-pointer
    )
    target_link_options(${target} INTERFACE
        -fsanitize=address,undefined
    )
endfunction()
