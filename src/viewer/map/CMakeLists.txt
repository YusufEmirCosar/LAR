# LAR map asset and projection model. This target has no widget or OpenGL
# dependency and is shared by the viewer, tests, and build-time asset tooling.
qt_add_library(lar-map-format STATIC
    src/viewer/map/map_mesh.h
    src/viewer/map/map_land_index.h
    src/viewer/map/map_land_index.cpp
    src/viewer/map/map_asset_limits.h
    src/viewer/map/map_asset_format.h
    src/viewer/map/map_checksum.h
    src/viewer/map/map_asset_reader.h
    src/viewer/map/map_asset_reader.cpp
    src/viewer/map/map_asset_source.h
    src/viewer/map/packaged_map_asset_source.h
    src/viewer/map/packaged_map_asset_source.cpp
    src/viewer/map/map_projection.h
    src/viewer/map/map_projection.cpp
)
target_include_directories(lar-map-format PUBLIC src)
target_link_libraries(lar-map-format PUBLIC Qt6::Core Qt6::Gui)

qt_add_library(lar-map-rendering STATIC
    src/viewer/map/map_palette.h
    src/viewer/map/map_shaders.h
    src/viewer/map/map_camera.h
    src/viewer/map/map_camera.cpp
    src/viewer/map/earth_map_gpu_renderer.h
    src/viewer/map/earth_map_gpu_renderer.cpp
    src/viewer/map/earth_map_widget.h
    src/viewer/map/earth_map_widget.cpp
)
target_include_directories(lar-map-rendering PUBLIC src)
target_link_libraries(lar-map-rendering
    PUBLIC
        lar-map-format
        Qt6::Widgets
        Qt6::OpenGLWidgets
)
