#pragma once

/**
 * @file glb_texture_reader.h
 * @brief Bounded decoder for glTF images, textures, and samplers.
 */

#include "viewer/plane/plane_model_mesh.h"

#include <QByteArray>
#include <QJsonArray>
#include <QString>

#include <vector>

/** @brief Converts validated glTF texture tables into immutable Plane texture values. */
class GlbTextureReader final {
  public:
    /**
     * @brief Decodes embedded PNG/JPEG images and their glTF sampler state.
     *
     * @param[in] bufferViews glTF buffer-view table.
     * @param[in] images glTF image table.
     * @param[in] textures glTF texture table.
     * @param[in] samplers glTF sampler table.
     * @param[in] binary GLB binary chunk referenced by the tables.
     * @param[in] mesh Destination mesh that receives the decoded textures.
     *
     * @return true when every declared texture is supported, bounded, and decoded.
     */
    static bool load(const QJsonArray &bufferViews, const QJsonArray &images,
                     const QJsonArray &textures, const QJsonArray &samplers,
                     const QByteArray &binary, PlaneModelMesh *mesh);

    /**
     * @brief Decodes textures from one or more glTF buffers and external image URIs.
     *
     * @param[in] bufferViews glTF buffer-view table.
     * @param[in] images glTF image table.
     * @param[in] textures glTF texture table.
     * @param[in] samplers glTF sampler table.
     * @param[in] binaries Resolved glTF buffer payloads.
     * @param[in] sourceDirectory Directory containing the source `.gltf` file.
     * @param[in] mesh Mesh that receives the decoded textures.
     *
     * @return true when every declared texture is supported, bounded, and decoded.
     */
    static bool load(const QJsonArray &bufferViews, const QJsonArray &images,
                     const QJsonArray &textures, const QJsonArray &samplers,
                     const std::vector<QByteArray> &binaries, const QString &sourceDirectory,
                     PlaneModelMesh *mesh);
};
