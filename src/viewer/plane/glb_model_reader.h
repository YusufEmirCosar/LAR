#pragma once

/**
 * @file glb_model_reader.h
 * @brief Bounded reader for the static `.gltf`/`.glb` subset used by Plane.
 */

#include "viewer/plane/plane_model_mesh.h"

#include <QString>

#include <memory>

struct GlbModelReadResult final {
    std::shared_ptr<const PlaneModelMesh> mesh;
    QString message;

    /**
     * @brief Reports whether reading produced a validated model mesh.
     */
    [[nodiscard]] bool succeeded() const noexcept {
        return mesh != nullptr;
    }
};

/**
 * @brief Parses the bounded static glTF 2.0 subset used by the plane renderer.
 *
 * The reader accepts `.gltf` and `.glb`, rejects required extensions, and
 * flattens the default scene's node matrix or TRS hierarchy. A primitive must
 * use `TRIANGLES`, float `POSITION` and `NORMAL` `VEC3` accessors, and optional
 * float `TEXCOORD_0` `VEC2` data. Indices are optional `UNSIGNED_BYTE`,
 * `UNSIGNED_SHORT`, or `UNSIGNED_INT` scalars. Sparse accessors and reused,
 * cyclic, or deeper-than-128 node graphs are rejected.
 *
 * Materials may supply a base-color factor, a texCoord-0 base-color texture,
 * and `doubleSided`. On success, transforms are baked into the vertices, the
 * axis-aligned bounds are recentered at the origin, and the largest extent is
 * uniformly scaled to two scene units. Resource, vertex, and index budgets are
 * validated before a mesh is returned.
 */
class GlbModelReader final {
  public:
    /** Returns either a fully validated immutable mesh or a user-facing failure message. */
    [[nodiscard]] static GlbModelReadResult readFile(const QString &path);
};
