qt_add_library(lar-map-asset-compiler-support STATIC
    tools/map_asset/source_map.h
    tools/map_asset/map_source_limits.h
    tools/map_asset/geojson_source_reader.h
    tools/map_asset/geojson_source_reader.cpp
    tools/map_asset/polygon_triangulator.h
    tools/map_asset/polygon_triangulator.cpp
    tools/map_asset/map_mesh_compiler.h
    tools/map_asset/map_mesh_compiler.cpp
    tools/map_asset/map_asset_writer.h
    tools/map_asset/map_asset_writer.cpp
)
target_include_directories(lar-map-asset-compiler-support
    PUBLIC
        src
        tools/map_asset
)
target_link_libraries(lar-map-asset-compiler-support
    PUBLIC
        lar-map-format
        Qt6::Core
)

qt_add_executable(lar-map-asset-compiler
    tools/map_asset/main.cpp
)
target_link_libraries(lar-map-asset-compiler PRIVATE
    lar-map-asset-compiler-support
    Qt6::Core
)

if(LAR_BUILD_FUZZERS)
    lar_instrument_fuzzed_library(lar-map-format)
    lar_instrument_fuzzed_library(lar-map-asset-compiler-support)

    set(map_reader_sources tests/fuzz/map_asset_reader_fuzz.cpp)
    set(map_source_sources tests/fuzz/geojson_source_reader_fuzz.cpp)
    if(NOT LAR_HAS_LIBFUZZER)
        list(APPEND map_reader_sources tests/fuzz/fuzz_standalone_main.cpp)
        list(APPEND map_source_sources tests/fuzz/fuzz_standalone_main.cpp)
    endif()

    qt_add_executable(lar-map-reader-fuzz ${map_reader_sources})
    target_link_libraries(lar-map-reader-fuzz PRIVATE
        lar-map-format
        Qt6::Core
    )
    lar_configure_fuzzer(lar-map-reader-fuzz)

    qt_add_executable(lar-map-source-fuzz ${map_source_sources})
    target_link_libraries(lar-map-source-fuzz PRIVATE
        lar-map-asset-compiler-support
        Qt6::Core
    )
    lar_configure_fuzzer(lar-map-source-fuzz)
endif()
