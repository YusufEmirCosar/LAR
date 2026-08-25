include_guard(GLOBAL)

function(lar_configure_map_assets)
    set(LAR_MAP_SOURCE_FILE
        "${PROJECT_SOURCE_DIR}/assets/map/world_boundaries.geojson"
        PARENT_SCOPE)
    set(LAR_GENERATED_MAP_DIR
        "${CMAKE_BINARY_DIR}/generated/map"
        PARENT_SCOPE)
    set(LAR_MAP_ASSET_FILE
        "${CMAKE_BINARY_DIR}/generated/map/lar_world_map.larmap"
        PARENT_SCOPE)
    set(LAR_MAP_MANIFEST_FILE
        "${CMAKE_BINARY_DIR}/generated/map/lar_world_map.manifest.json"
        PARENT_SCOPE)
endfunction()

function(lar_define_map_asset_target)
    add_custom_command(
        OUTPUT
            "${LAR_MAP_ASSET_FILE}"
            "${LAR_MAP_MANIFEST_FILE}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${LAR_GENERATED_MAP_DIR}"
        COMMAND "$<TARGET_FILE:lar-map-asset-compiler>"
            "${LAR_MAP_SOURCE_FILE}"
            "${LAR_MAP_ASSET_FILE}"
            "${LAR_MAP_MANIFEST_FILE}"
        DEPENDS
            lar-map-asset-compiler
            "${LAR_MAP_SOURCE_FILE}"
        COMMENT "Compiling the packaged LAR Earth map"
        VERBATIM
    )
    add_custom_target(lar-map-asset
        DEPENDS "${LAR_MAP_ASSET_FILE}" "${LAR_MAP_MANIFEST_FILE}"
    )
endfunction()

function(lar_copy_map_package target_name)
    add_dependencies(${target_name} lar-map-asset)
    set_property(TARGET ${target_name} APPEND PROPERTY LINK_DEPENDS
        "${LAR_MAP_ASSET_FILE}"
        "${LAR_MAP_MANIFEST_FILE}"
    )
    add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${LAR_MAP_ASSET_FILE}"
            "$<TARGET_FILE_DIR:${target_name}>/lar_world_map.larmap"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${LAR_MAP_MANIFEST_FILE}"
            "$<TARGET_FILE_DIR:${target_name}>/lar_world_map.manifest.json"
        VERBATIM
    )
endfunction()
