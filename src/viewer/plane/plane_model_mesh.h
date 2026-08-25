#pragma once

/**
 * @file plane_model_mesh.h
 * @brief Validated, flattened CPU representation of the packaged F-16 model.
 */

#include <QImage>
#include <QVector4D>

#include <cstddef>
#include <cstdint>
#include <vector>

/** @brief Float count for one interleaved position, normal, and UV vertex. */
inline constexpr std::size_t PlaneModelVertexStrideFloats = 8U;

/** @brief Supported glTF texture-coordinate addressing modes. */
enum class PlaneTextureWrap { ClampToEdge, MirroredRepeat, Repeat };

/** @brief Supported glTF minification and mipmap sampling modes. */
enum class PlaneTextureMinFilter {
    Nearest,
    Linear,
    NearestMipmapNearest,
    LinearMipmapNearest,
    NearestMipmapLinear,
    LinearMipmapLinear
};

/** @brief Supported glTF magnification sampling modes. */
enum class PlaneTextureMagFilter { Nearest, Linear };

/** @brief Decoded base-color image and validated glTF sampler state. */
struct PlaneModelTexture final {
    QImage image;
    PlaneTextureWrap wrapS = PlaneTextureWrap::Repeat;
    PlaneTextureWrap wrapT = PlaneTextureWrap::Repeat;
    PlaneTextureMinFilter minFilter = PlaneTextureMinFilter::LinearMipmapLinear;
    PlaneTextureMagFilter magFilter = PlaneTextureMagFilter::Linear;
};

struct PlaneModelDrawRange final {
    std::size_t firstIndex = 0U;
    std::size_t indexCount = 0U;
    QVector4D baseColor{0.35F, 0.38F, 0.4F, 1.0F};
    int baseColorTexture = -1;
    bool doubleSided = false;
};

/** @brief Interleaved position/normal/UV vertices, textures, and material draw ranges. */
struct PlaneModelMesh final {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<PlaneModelDrawRange> draws;
    std::vector<PlaneModelTexture> textures;
    /** Nose-to-tail extent after model normalization, expressed in scene units. */
    float forwardExtentSceneUnits = 0.0F;

    /**
     * @brief Reports whether the model mesh has no renderable geometry.
     *
     * @return True when vertices, indices, or draw ranges are empty.
     */
    [[nodiscard]] bool empty() const noexcept {
        return vertices.empty() || indices.empty() || draws.empty();
    }
    [[nodiscard]] std::size_t vertexCount() const noexcept {
        return vertices.size() / PlaneModelVertexStrideFloats;
    }
};
