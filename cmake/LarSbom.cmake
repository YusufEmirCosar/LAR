include_guard(GLOBAL)

function(lar_configure_sbom)
    if(APPLE)
        set(LAR_QT_PLATFORM_PLUGIN "qcocoa")
    elseif(WIN32)
        set(LAR_QT_PLATFORM_PLUGIN "qwindows")
    else()
        set(LAR_QT_PLATFORM_PLUGIN "qxcb")
    endif()
    set(LAR_SBOM_FILE "${CMAKE_BINARY_DIR}/lar-sbom.spdx.json")
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/lar-sbom.spdx.json.in"
        "${LAR_SBOM_FILE}"
        @ONLY
    )
    add_custom_target(generate-sbom DEPENDS "${LAR_SBOM_FILE}")
    install(FILES "${LAR_SBOM_FILE}"
        DESTINATION share/lar-area-display
    )
    set(LAR_SBOM_FILE "${LAR_SBOM_FILE}" PARENT_SCOPE)
endfunction()
