include_guard(GLOBAL)

# Copies the packaged F-16, cubemaps, and compact water mask beside an executable.
function(lar_copy_plane_assets target_name)
    file(GLOB plane_cubemap_files CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/assets/cubemaps/*.png"
    )
    set_property(TARGET ${target_name} APPEND PROPERTY LINK_DEPENDS
        "${PROJECT_SOURCE_DIR}/assets/models/f16_3.glb"
        "${PROJECT_SOURCE_DIR}/assets/water/dted0_water_mask.bin"
        ${plane_cubemap_files}
    )
    add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            -DLAR_PLANE_ASSET_SOURCE=${PROJECT_SOURCE_DIR}/assets
            -DLAR_PLANE_ASSET_DESTINATION=$<TARGET_FILE_DIR:${target_name}>/assets
            -DLAR_PLANE_ASSET_LOCK_ROOT=${PROJECT_BINARY_DIR}/plane-assets-locks
            -P "${PROJECT_SOURCE_DIR}/cmake/StagePlaneAssets.cmake"
        VERBATIM
    )
endfunction()
