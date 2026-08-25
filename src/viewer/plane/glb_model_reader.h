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
     *
     * @return True when the result contains a model mesh.
     */
    [[nodiscard]] bool succeeded() const noexcept {
        return mesh != nullptr;
    }
};

/** @brief Parses, validates, flattens, recenters, and normalizes a static glTF scene. */
class GlbModelReader final {
  public:
    [[nodiscard]] static GlbModelReadResult readFile(const QString &path);
};
