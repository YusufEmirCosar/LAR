#pragma once

/**
 * @file map_mesh_compiler.h
 * @brief Conversion from source polygons to renderer-ready map meshes.
 */

#include "source_map.h"
#include "viewer/map/map_mesh.h"

#include <QString>

namespace lar::map::tool {

/** @brief Compiled map mesh and an optional failure diagnostic. */
struct MapMeshCompileResult final {
    MapMesh mesh;
    QString message;

    /**
     * @brief Reports whether compilation produced a non-empty mesh.
     */
    [[nodiscard]] bool succeeded() const noexcept {
        return !mesh.empty();
    }
};

/** @brief Triangulates polygons and builds flat, sphere, and border indices. */
class MapMeshCompiler final {
  public:
    static MapMeshCompileResult compile(const SourceMap &source);
};

} // namespace lar::map::tool
