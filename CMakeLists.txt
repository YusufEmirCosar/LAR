cmake_minimum_required(VERSION 3.24)

project(DynamicLarSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LAR_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(LAR_ENABLE_STRICT_CONVERSIONS
    "Enable conversion, sign-conversion, and shadow warnings" OFF)
option(LAR_ENABLE_HARDENING "Enable supported compiler/linker hardening" ON)
option(LAR_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LAR_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(LAR_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(LAR_ENABLE_COVERAGE "Enable GCC/gcov coverage instrumentation" OFF)
option(LAR_ENABLE_MUTATION "Enable Mull mutation instrumentation" OFF)
option(LAR_BUILD_FUZZERS "Build parser fuzz targets" OFF)
option(LAR_ENABLE_QUALITY_TARGETS "Define Python/tool-based quality targets" ON)
include(CTest)
set(LAR_DTED0_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/assets/DTED0" CACHE PATH
    "Development DTED0 root used by Plane terrain (not copied into build outputs)")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LarWarnings)
include(LarSanitizers)
include(LarCoverage)
include(LarMutation)
include(LarHardening)
include(LarFuzzing)
include(LarMapAssets)
include(LarPlaneAssets)
include(LarSbom)

lar_configure_project_options()
lar_configure_sanitizers()
lar_configure_coverage()
lar_configure_mutation()
lar_configure_hardening()
lar_detect_libfuzzer()
lar_configure_map_assets()

find_package(Qt6 6.10.3 REQUIRED COMPONENTS
    Core
    Concurrent
    Gui
    Network
    Widgets
    OpenGLWidgets
)
if(BUILD_TESTING)
    find_package(Qt6 6.10.3 REQUIRED COMPONENTS Test)
endif()
lar_configure_sbom()

include("${PROJECT_SOURCE_DIR}/src/viewer/map/CMakeLists.txt")
include("${PROJECT_SOURCE_DIR}/tools/CMakeLists.txt")
include("${PROJECT_SOURCE_DIR}/src/domain/CMakeLists.txt")
include("${PROJECT_SOURCE_DIR}/src/application/CMakeLists.txt")
include("${PROJECT_SOURCE_DIR}/src/infrastructure/CMakeLists.txt")
include("${PROJECT_SOURCE_DIR}/src/viewer/CMakeLists.txt")
if(BUILD_TESTING)
    include("${PROJECT_SOURCE_DIR}/tests/CMakeLists.txt")
endif()

lar_apply_project_defaults()
if(LAR_ENABLE_QUALITY_TARGETS)
    include(LarQualityTargets)
endif()

file(COPY maps DESTINATION "${CMAKE_BINARY_DIR}")

install(TARGETS lar-viewer lar-test-sender
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
if(WIN32)
    qt_generate_deploy_app_script(
        TARGET lar-viewer
        OUTPUT_SCRIPT lar_viewer_deploy_script
        NO_TRANSLATIONS
    )
    install(SCRIPT "${lar_viewer_deploy_script}")
endif()
install(FILES
    "${LAR_MAP_ASSET_FILE}"
    "${LAR_MAP_MANIFEST_FILE}"
    DESTINATION bin
)
install(FILES
    assets/models/f16_3.glb
    DESTINATION bin/assets/models
)
install(DIRECTORY
    assets/cubemaps/
    DESTINATION bin/assets/cubemaps
    FILES_MATCHING PATTERN "*.png"
)
install(FILES
    maps/full-state.json
    maps/dlz-inputs.json
    maps/full-state-dlz.json
    DESTINATION share/lar-area-display/maps
)
