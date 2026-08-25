include_guard(GLOBAL)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(GLOB_RECURSE LAR_FORMAT_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.h"
    "${PROJECT_SOURCE_DIR}/tests/*.cpp"
    "${PROJECT_SOURCE_DIR}/tests/*.h"
    "${PROJECT_SOURCE_DIR}/tools/*.cpp"
    "${PROJECT_SOURCE_DIR}/tools/*.h"
)

find_program(LAR_CLANG_FORMAT NAMES clang-format clang-format-18 clang-format-17)
if(APPLE AND NOT LAR_CLANG_FORMAT)
    execute_process(
        COMMAND xcrun --find clang-format
        OUTPUT_VARIABLE LAR_CLANG_FORMAT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(LAR_CLANG_FORMAT)
    add_custom_target(check-format
        COMMAND "${LAR_CLANG_FORMAT}" --dry-run --Werror ${LAR_FORMAT_SOURCES}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Checking C++ formatting"
        VERBATIM
    )
else()
    add_custom_target(check-format
        COMMAND "${CMAKE_COMMAND}" -E echo
            "clang-format is required for check-format"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

find_program(LAR_CLANG_TIDY NAMES clang-tidy clang-tidy-18 clang-tidy-17)
file(GLOB_RECURSE LAR_TIDY_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/tools/*.cpp"
)
if(LAR_CLANG_TIDY)
    add_custom_target(check-tidy
        COMMAND "${LAR_CLANG_TIDY}"
            -p="${CMAKE_BINARY_DIR}"
            --warnings-as-errors=*
            ${LAR_TIDY_SOURCES}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Running clang-tidy"
        VERBATIM
    )
else()
    add_custom_target(check-tidy
        COMMAND "${CMAKE_COMMAND}" -E echo
            "clang-tidy is required for check-tidy"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

add_custom_target(check-architecture
    COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/check_architecture.py"
        "${PROJECT_SOURCE_DIR}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(check-doc-links
    COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/check_doc_links.py"
        "${PROJECT_SOURCE_DIR}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(check-doc-coverage
    COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/check_doxygen_coverage.py"
        "${PROJECT_SOURCE_DIR}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(check-doc-quality
    COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/check_doc_quality.py"
        "${PROJECT_SOURCE_DIR}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)

find_package(Doxygen QUIET)
if(Doxygen_FOUND)
    configure_file(
        "${PROJECT_SOURCE_DIR}/docs/Doxyfile.in"
        "${CMAKE_BINARY_DIR}/Doxyfile"
        @ONLY
    )
    add_custom_target(check-docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Building warning-free API documentation"
        VERBATIM
    )
else()
    add_custom_target(check-docs
        COMMAND "${CMAKE_COMMAND}" -E echo
            "Doxygen is required for check-docs"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

set(lar_install_smoke_prefix "${CMAKE_BINARY_DIR}/install-smoke")
add_custom_target(check-install-layout
    COMMAND "${CMAKE_COMMAND}" -E remove_directory
        "${lar_install_smoke_prefix}"
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
        --prefix "${lar_install_smoke_prefix}"
    COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/check_install_layout.py"
        "${lar_install_smoke_prefix}"
    DEPENDS lar-viewer lar-test-sender lar-map-asset generate-sbom
    USES_TERMINAL
    VERBATIM
)

set(LAR_PERFORMANCE_BASELINE "" CACHE FILEPATH
    "Pinned-runner JSON baseline used by check-performance")
if(LAR_PERFORMANCE_BASELINE)
    add_custom_target(check-performance
        COMMAND lar-performance-benchmarks
            --output "${CMAKE_BINARY_DIR}/performance-current.json"
        COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/check_performance.py"
            "${LAR_PERFORMANCE_BASELINE}"
            "${CMAKE_BINARY_DIR}/performance-current.json"
            10
        DEPENDS lar-performance-benchmarks
        USES_TERMINAL
        VERBATIM
    )
else()
    add_custom_target(check-performance
        COMMAND "${CMAKE_COMMAND}" -E echo
            "Set LAR_PERFORMANCE_BASELINE to the pinned-runner JSON baseline"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

find_program(LAR_GCOVR NAMES gcovr)
if(LAR_ENABLE_COVERAGE AND LAR_GCOVR)
    add_custom_target(coverage-report
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
            -LE "network|gpu|slow"
        COMMAND "${LAR_GCOVR}"
            --root "${PROJECT_SOURCE_DIR}"
            --filter "${PROJECT_SOURCE_DIR}/src"
            --json-summary-pretty
            --output "${CMAKE_BINARY_DIR}/coverage-summary.json"
            "${CMAKE_BINARY_DIR}"
        COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/enforce_coverage.py"
            "${CMAKE_BINARY_DIR}/coverage-summary.json"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        USES_TERMINAL
        VERBATIM
    )
else()
    add_custom_target(coverage-report
        COMMAND "${CMAKE_COMMAND}" -E echo
            "coverage-report requires LAR_ENABLE_COVERAGE=ON and gcovr"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

find_program(LAR_MULL_RUNNER NAMES
    mull-runner-19 mull-runner-18 mull-runner-17 mull-runner)
if(LAR_ENABLE_MUTATION AND LAR_MULL_RUNNER)
    add_custom_target(check-mutation
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_BINARY_DIR}/mutation-report"
        COMMAND "${LAR_MULL_RUNNER}"
            --strict
            --mutation-score-threshold 85
            --reporters IDE
            --reporters Elements
            --report-dir "${CMAKE_BINARY_DIR}/mutation-report"
            --report-name lar-domain-application
            "$<TARGET_FILE:lardomain-tests>"
        DEPENDS lardomain-tests
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM
    )
else()
    add_custom_target(check-mutation
        COMMAND "${CMAKE_COMMAND}" -E echo
            "check-mutation requires LAR_ENABLE_MUTATION=ON and mull-runner"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

add_custom_target(check-repository
    DEPENDS check-format check-architecture check-doc-links check-doc-coverage
            check-doc-quality check-docs
)
