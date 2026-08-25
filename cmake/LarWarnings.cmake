include_guard(GLOBAL)

function(lar_configure_project_options)
    add_library(lar-project-options INTERFACE)
    target_compile_features(lar-project-options INTERFACE cxx_std_17)

    add_library(lar-project-warnings INTERFACE)
    if(MSVC)
        target_compile_options(lar-project-warnings INTERFACE
            /W4
            /permissive-
            $<$<BOOL:${LAR_WARNINGS_AS_ERRORS}>:/WX>
        )
    else()
        target_compile_options(lar-project-warnings INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            $<$<BOOL:${LAR_WARNINGS_AS_ERRORS}>:-Werror>
            $<$<AND:$<BOOL:${LAR_ENABLE_STRICT_CONVERSIONS}>,$<COMPILE_LANGUAGE:CXX>>:-Wconversion>
            $<$<AND:$<BOOL:${LAR_ENABLE_STRICT_CONVERSIONS}>,$<COMPILE_LANGUAGE:CXX>>:-Wsign-conversion>
            $<$<AND:$<BOOL:${LAR_ENABLE_STRICT_CONVERSIONS}>,$<COMPILE_LANGUAGE:CXX>>:-Wshadow>
        )
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(lar-project-warnings INTERFACE
                -Wno-variadic-macro-arguments-omitted
            )
        endif()
    endif()
endfunction()

function(lar_apply_project_defaults)
    get_property(project_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(project_target IN LISTS project_targets)
        get_target_property(target_type ${project_target} TYPE)
        get_target_property(imported ${project_target} IMPORTED)
        if(imported
           OR target_type STREQUAL "UTILITY"
           OR target_type STREQUAL "INTERFACE_LIBRARY"
           OR project_target MATCHES "^lar-project-")
            continue()
        endif()
        target_link_libraries(${project_target} PRIVATE
            lar-project-options
            lar-project-warnings
            lar-project-sanitizers
            lar-project-coverage
            lar-project-mutation
            lar-project-hardening
        )
    endforeach()
endfunction()
