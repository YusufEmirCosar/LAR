add_test(NAME lar-testsender-scenario-tests COMMAND lar-testsender-scenario-tests)
set_tests_properties(lar-testsender-scenario-tests PROPERTIES TIMEOUT 30)

qt_add_executable(lardomain-tests tests/domain_tests.cpp)
target_link_libraries(lardomain-tests PRIVATE
    lar-infrastructure-qt
    lar-domain
    Qt6::Test
)
add_test(NAME lardomain-tests COMMAND lardomain-tests)
set_tests_properties(lardomain-tests PROPERTIES TIMEOUT 30)

qt_add_executable(lardlz-tests tests/dlz_tests.cpp)
target_link_libraries(lardlz-tests PRIVATE
    lar-presentation-widgets
    lar-domain
    Qt6::Widgets
    Qt6::Test
)
add_test(NAME lardlz-tests COMMAND lardlz-tests)
set_tests_properties(lardlz-tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

qt_add_executable(lardlz-view-tests tests/dlz_view_tests.cpp)
target_link_libraries(lardlz-view-tests PRIVATE
    lar-presentation-widgets
    lar-domain
    Qt6::Widgets
    Qt6::Test
)
add_test(NAME lardlz-view-tests COMMAND lardlz-view-tests)
set_tests_properties(lardlz-view-tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

qt_add_executable(lararchitecture-tests tests/architecture_tests.cpp)
target_include_directories(lararchitecture-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(lararchitecture-tests PRIVATE
    lar-infrastructure-qt
    lar-application
    lar-domain
    Qt6::Test
)
add_test(NAME lararchitecture-tests COMMAND lararchitecture-tests)
set_tests_properties(lararchitecture-tests PROPERTIES TIMEOUT 30)

qt_add_executable(larrecording-pipeline-tests
    tests/recording_pipeline_tests.cpp
)
target_include_directories(larrecording-pipeline-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(larrecording-pipeline-tests PRIVATE
    lar-infrastructure-qt
    lar-application
    lar-domain
    Qt6::Test
)
add_test(NAME larrecording-pipeline-tests COMMAND larrecording-pipeline-tests)
set_tests_properties(larrecording-pipeline-tests PROPERTIES
    LABELS "unit;contract"
    TIMEOUT 30
)

qt_add_executable(larplayback-service-tests
    tests/playback_service_tests.cpp
)
target_include_directories(larplayback-service-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(larplayback-service-tests PRIVATE
    lar-infrastructure-qt
    lar-application
    lar-domain
    Qt6::Test
)
add_test(NAME larplayback-service-tests COMMAND larplayback-service-tests)
set_tests_properties(larplayback-service-tests PROPERTIES
    LABELS "unit;contract"
    TIMEOUT 30
)

qt_add_executable(larsession-adapter-tests
    tests/session_adapter_tests.cpp
)
target_include_directories(larsession-adapter-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(larsession-adapter-tests PRIVATE
    lar-infrastructure-qt
    lar-application
    lar-domain
    Qt6::Test
)
add_test(NAME larsession-adapter-tests COMMAND larsession-adapter-tests)
set_tests_properties(larsession-adapter-tests PROPERTIES
    LABELS "unit;integration"
    TIMEOUT 30
)

qt_add_executable(larnetwork-adapter-tests
    tests/network_adapter_tests.cpp
)
target_include_directories(larnetwork-adapter-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(larnetwork-adapter-tests PRIVATE
    lar-infrastructure-qt
    lar-application
    lar-domain
    Qt6::Network
    Qt6::Test
)
add_test(NAME larnetwork-adapter-tests COMMAND larnetwork-adapter-tests)
set_tests_properties(larnetwork-adapter-tests PROPERTIES
    LABELS "integration;network"
    TIMEOUT 30
)

qt_add_executable(laroperational-soak-tests
    tests/operational_soak_tests.cpp
    src/testsender/scenarios.cpp
)
target_compile_definitions(laroperational-soak-tests PRIVATE
    "LAR_TEST_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
)
target_link_libraries(laroperational-soak-tests PRIVATE
    lar-infrastructure-qt
    lar-application
    lar-domain
    Qt6::Network
    Qt6::Test
)
add_test(NAME laroperational-soak-tests COMMAND laroperational-soak-tests)
set_tests_properties(laroperational-soak-tests PROPERTIES
    LABELS "integration;network;slow"
    TIMEOUT 1900
)

qt_add_executable(larsource-lifecycle-tests
    tests/source_lifecycle_tests.cpp
    tests/support/controlled_application_runtime.h
)
target_include_directories(larsource-lifecycle-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(larsource-lifecycle-tests PRIVATE
    lar-application
    Qt6::Test
)
add_test(NAME larsource-lifecycle-tests COMMAND larsource-lifecycle-tests)
set_tests_properties(larsource-lifecycle-tests PROPERTIES
    LABELS "unit;contract"
    TIMEOUT 30
)

qt_add_executable(larsession-time-tests tests/session_time_tests.cpp)
target_link_libraries(larsession-time-tests PRIVATE
    lar-presentation-widgets
    lar-application
    Qt6::Test
)
add_test(NAME larsession-time-tests COMMAND larsession-time-tests)
set_tests_properties(larsession-time-tests PROPERTIES
    LABELS "unit"
    TIMEOUT 30
)

qt_add_executable(larruntime-contract-tests
    tests/runtime_contract_tests.cpp
    tests/support/fake_datagram_source.h
    tests/support/direct_runtime_harness.h
)
target_include_directories(larruntime-contract-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(larruntime-contract-tests PRIVATE
    lar-infrastructure-qt
    lar-application
    Qt6::Test
)
add_test(NAME larruntime-contract-tests COMMAND larruntime-contract-tests)
set_tests_properties(larruntime-contract-tests PROPERTIES
    LABELS "contract;integration"
    TIMEOUT 30
)

qt_add_executable(larrequest-gate-tests
    tests/request_result_gate_tests.cpp
)
target_link_libraries(larrequest-gate-tests PRIVATE
    lar-application
    Qt6::Test
)
add_test(NAME larrequest-gate-tests COMMAND larrequest-gate-tests)
set_tests_properties(larrequest-gate-tests PROPERTIES
    LABELS "unit;contract"
    TIMEOUT 30
)

qt_add_executable(lar-ip-access-policy-tests tests/ip_access_policy_tests.cpp)
target_link_libraries(lar-ip-access-policy-tests PRIVATE
    lar-infrastructure-qt
    lar-application
    Qt6::Network
    Qt6::Test
)
add_test(NAME lar-ip-access-policy-tests COMMAND lar-ip-access-policy-tests)
set_tests_properties(lar-ip-access-policy-tests PROPERTIES TIMEOUT 30)

qt_add_executable(larmap-tests tests/map_asset_tests.cpp)
target_link_libraries(larmap-tests PRIVATE lar-map-format Qt6::Test)
add_dependencies(larmap-tests lar-map-asset)
target_compile_definitions(
    larmap-tests
    PRIVATE
        "LAR_TEST_MAP_PACKAGE_DIR=\"${LAR_GENERATED_MAP_DIR}\""
)
add_test(NAME larmap-tests COMMAND larmap-tests)
set_tests_properties(larmap-tests PROPERTIES TIMEOUT 30)

qt_add_executable(larmap-compiler-tests
    tests/map_compiler_tests.cpp
)
target_link_libraries(larmap-compiler-tests PRIVATE
    lar-map-asset-compiler-support
    Qt6::Test
)
add_test(NAME larmap-compiler-tests COMMAND larmap-compiler-tests)
set_tests_properties(larmap-compiler-tests PROPERTIES TIMEOUT 30)

qt_add_executable(larmap-renderer-validation-tests
    tests/map_renderer_validation_tests.cpp
)
target_link_libraries(larmap-renderer-validation-tests PRIVATE
    lar-map-rendering
    Qt6::Test
)
add_test(
    NAME larmap-renderer-validation-tests
    COMMAND larmap-renderer-validation-tests
)
set_tests_properties(
    larmap-renderer-validation-tests
    PROPERTIES TIMEOUT 30
)

qt_add_executable(larzone-tests tests/lar_zone_mesh_tests.cpp)
target_link_libraries(larzone-tests PRIVATE
    lar-presentation-widgets
    lar-domain
    lar-map-format
    Qt6::Test
)
add_test(NAME larzone-tests COMMAND larzone-tests)
set_tests_properties(larzone-tests PROPERTIES TIMEOUT 30)

qt_add_executable(larpolar-udp-tests
    tests/polar_udp_tests.cpp
    src/testsender/scenarios.h
    src/testsender/scenarios.cpp
)
target_link_libraries(larpolar-udp-tests PRIVATE
    lar-presentation-widgets
    lar-infrastructure-qt
    lar-application
    lar-domain
    Qt6::Network
    Qt6::Test
)
target_compile_definitions(
    larpolar-udp-tests
    PRIVATE
        "LAR_TEST_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
)
add_test(NAME larpolar-udp-tests COMMAND larpolar-udp-tests)
set_tests_properties(larpolar-udp-tests PROPERTIES TIMEOUT 30)

qt_add_executable(larcamera-tests tests/viewport_camera_tests.cpp)
target_link_libraries(larcamera-tests PRIVATE
    lar-presentation-widgets
    lar-domain
    Qt6::Test
)
add_test(NAME larcamera-tests COMMAND larcamera-tests)
set_tests_properties(larcamera-tests PROPERTIES TIMEOUT 30)

qt_add_executable(larmap-loading-tests tests/map_loading_tests.cpp)
target_link_libraries(larmap-loading-tests PRIVATE
    lar-presentation-widgets
    lar-map-format
    Qt6::Widgets
    Qt6::Test
)
add_test(NAME larmap-loading-tests COMMAND larmap-loading-tests)
set_tests_properties(larmap-loading-tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

qt_add_executable(larviewport-page-tests
    tests/viewport_page_tests.cpp
)
target_link_libraries(larviewport-page-tests PRIVATE
    lar-presentation-widgets
    lar-map-format
    Qt6::Widgets
    Qt6::Test
)
add_test(NAME larviewport-page-tests COMMAND larviewport-page-tests)
set_tests_properties(larviewport-page-tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

qt_add_executable(larviewport-worker-tests
    tests/viewport_worker_tests.cpp
)

qt_add_executable(larplane-view-tests
    tests/plane_view_tests.cpp
)
target_compile_definitions(larplane-view-tests PRIVATE
    "LAR_TEST_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
)
target_link_libraries(larplane-view-tests PRIVATE
    lar-presentation-widgets
    lar-domain
    Qt6::Widgets
    Qt6::Test
)
add_test(NAME larplane-view-tests COMMAND larplane-view-tests)
set_tests_properties(larplane-view-tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "unit;integration"
    TIMEOUT 30
)
target_link_libraries(larviewport-worker-tests PRIVATE
    lar-presentation-widgets
    lar-domain
    Qt6::Test
)
add_test(NAME larviewport-worker-tests COMMAND larviewport-worker-tests)
set_tests_properties(larviewport-worker-tests PROPERTIES
    LABELS "unit;concurrency"
    TIMEOUT 30
)

qt_add_executable(lar-ui-snapshot EXCLUDE_FROM_ALL
    tests/ui_snapshot_tool.cpp
    assets/ui/icons/icons.qrc
)
target_link_libraries(lar-ui-snapshot PRIVATE
    lar-presentation-widgets
    lar-infrastructure-qt
    lar-application
    lar-domain
    Qt6::Widgets
)
lar_copy_map_package(lar-ui-snapshot)
lar_copy_plane_assets(lar-ui-snapshot)

qt_add_executable(larviewer-tests
    tests/viewer_tests.cpp
    assets/ui/icons/icons.qrc
)
target_link_libraries(larviewer-tests PRIVATE
    lar-presentation-widgets
    lar-infrastructure-qt
    lar-application
    lar-domain
    Qt6::Widgets
    Qt6::Test
)
add_test(NAME larviewer-tests COMMAND larviewer-tests)
lar_copy_plane_assets(larviewer-tests)
set_tests_properties(larviewer-tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)

qt_add_executable(lar-earth-view-gpu-tests
    tests/earth_lar_view_gpu_tests.cpp
)
target_link_libraries(lar-earth-view-gpu-tests PRIVATE
    lar-presentation-widgets
    lar-domain
    Qt6::Widgets
)
lar_copy_map_package(lar-earth-view-gpu-tests)
add_test(
    NAME lar-earth-view-gpu-tests
    COMMAND lar-earth-view-gpu-tests
)
set_tests_properties(lar-earth-view-gpu-tests PROPERTIES
    SKIP_RETURN_CODE 77
    TIMEOUT 30
)

qt_add_executable(lar-plane-view-gpu-tests
    tests/plane_view_gpu_tests.cpp
)
target_link_libraries(lar-plane-view-gpu-tests PRIVATE
    lar-presentation-widgets
    lar-domain
    Qt6::Widgets
)
lar_copy_plane_assets(lar-plane-view-gpu-tests)
add_test(NAME lar-plane-view-gpu-tests COMMAND lar-plane-view-gpu-tests)
set_tests_properties(lar-plane-view-gpu-tests PROPERTIES
    SKIP_RETURN_CODE 77
    LABELS "gpu;integration"
    TIMEOUT 30
)

qt_add_executable(lar-performance-benchmarks EXCLUDE_FROM_ALL
    tests/performance_benchmarks.cpp
    src/testsender/scenarios.cpp
)
target_compile_definitions(lar-performance-benchmarks PRIVATE
    "LAR_TEST_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
    "LAR_TEST_MAP_PACKAGE_DIR=\"${LAR_GENERATED_MAP_DIR}\""
)
target_link_libraries(lar-performance-benchmarks PRIVATE
    lar-presentation-widgets
    lar-infrastructure-qt
    lar-application
    lar-domain
    lar-map-format
    Qt6::Core
)
add_dependencies(lar-performance-benchmarks lar-map-asset)
add_custom_target(run-benchmarks
    COMMAND lar-performance-benchmarks
        --output "${CMAKE_BINARY_DIR}/performance-current.json"
    DEPENDS lar-performance-benchmarks
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    USES_TERMINAL
    VERBATIM
)

set_tests_properties(
    lar-testsender-scenario-tests
    lardomain-tests
    lardlz-tests
    larzone-tests
    larcamera-tests
    larsession-time-tests
    PROPERTIES LABELS "unit"
)
set_tests_properties(
    lararchitecture-tests
    larrecording-pipeline-tests
    larplayback-service-tests
    larsource-lifecycle-tests
    larruntime-contract-tests
    larrequest-gate-tests
    lar-ip-access-policy-tests
    PROPERTIES LABELS "unit;contract"
)
set_tests_properties(
    larsession-adapter-tests
    larmap-tests
    larmap-compiler-tests
    larmap-loading-tests
    larviewport-page-tests
    larviewer-tests
    lardlz-view-tests
    PROPERTIES LABELS "integration"
)
set_property(TEST larmap-loading-tests APPEND PROPERTY LABELS concurrency)
set_tests_properties(
    larnetwork-adapter-tests
    larpolar-udp-tests
    PROPERTIES LABELS "integration;network"
)
set_tests_properties(
    larmap-renderer-validation-tests
    lar-earth-view-gpu-tests
    PROPERTIES LABELS "gpu;integration"
)

if(LAR_ENABLE_ASAN OR LAR_ENABLE_UBSAN OR LAR_ENABLE_TSAN)
    get_property(lar_registered_tests DIRECTORY PROPERTY TESTS)
    foreach(lar_registered_test IN LISTS lar_registered_tests)
        set_property(TEST ${lar_registered_test} APPEND PROPERTY LABELS sanitizer)
    endforeach()
endif()

if(LAR_ENABLE_TSAN)
    get_property(lar_tsan_tests DIRECTORY PROPERTY TESTS)
    foreach(lar_tsan_test IN LISTS lar_tsan_tests)
        set_property(TEST ${lar_tsan_test} APPEND PROPERTY ENVIRONMENT
            "TSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/tests/tsan_suppressions.txt:halt_on_error=1"
        )
    endforeach()
endif()

if(LAR_BUILD_FUZZERS)
    lar_instrument_fuzzed_library(lar-domain)
    lar_instrument_fuzzed_library(lar-application)
    lar_instrument_fuzzed_library(lar-infrastructure-qt)

    function(lar_add_parser_fuzzer target source)
        set(fuzzer_sources "${source}")
        if(NOT LAR_HAS_LIBFUZZER)
            list(APPEND fuzzer_sources tests/fuzz/fuzz_standalone_main.cpp)
        endif()
        qt_add_executable(${target} ${fuzzer_sources})
        target_link_libraries(${target} PRIVATE
            lar-infrastructure-qt
            lar-application
            lar-domain
            Qt6::Core
        )
        lar_configure_fuzzer(${target})
    endfunction()

    lar_add_parser_fuzzer(
        lar-mapping-repository-fuzz
        tests/fuzz/mapping_repository_fuzz.cpp
    )
    lar_add_parser_fuzzer(
        lar-packet-decoder-fuzz
        tests/fuzz/packet_decoder_fuzz.cpp
    )
    lar_add_parser_fuzzer(
        lar-session-reader-fuzz
        tests/fuzz/session_reader_fuzz.cpp
    )
    lar_add_parser_fuzzer(
        lar-ip-policy-fuzz
        tests/fuzz/ip_policy_fuzz.cpp
    )
    lar_add_parser_fuzzer(
        lar-dlz-solver-fuzz
        tests/fuzz/dlz_solver_fuzz.cpp
    )
endif()
