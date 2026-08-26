#pragma once

/**
 * @file plane_scene_renderer.h
 * @brief OpenGL resources and draw passes for the centered F-16 simulation.
 */

#include "viewer/plane/cubemap_catalog.h"
#include "viewer/plane/plane_model_mesh.h"
#include "viewer/plane/plane_surface_gpu_layer.h"
#include "viewer/plane/plane_terrain_gpu_layer.h"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QQuaternion>

#include <memory>
#include <vector>

class QOpenGLShaderProgram;
class QOpenGLTexture;

/** @brief Owns all context-bound Plane workspace GPU resources. */
class PlaneSceneRenderer final : protected QOpenGLFunctions {
  public:
    PlaneSceneRenderer();
    ~PlaneSceneRenderer();
    PlaneSceneRenderer(const PlaneSceneRenderer &other) = delete;
    PlaneSceneRenderer &operator=(const PlaneSceneRenderer &other) = delete;

    void setModel(std::shared_ptr<const PlaneModelMesh> model);
    bool setSkybox(const CubemapFaces &faces, QString *errorMessage = nullptr);
    void setSurfaceState(const PlaneSurfaceState &state) noexcept;
    void setSurfaceVisible(bool visible) noexcept;
    /** @brief Sets the immutable terrain snapshot uploaded on the next paint pass. */
    void setTerrainPatch(PlaneTerrainPatchPtr patch) noexcept;
    /** @brief Shows DTED terrain; no land-colored fallback is drawn while a patch is loading. */
    void setTerrainVisible(bool visible) noexcept;
    /** @brief Positions and scales the stable terrain anchor relative to the current aircraft. */
    void setTerrainPlacement(const QVector2D &offsetXZ, const QVector2D &scaleXZ,
                             float aircraftAltitudeScene) noexcept;
    /** @brief Positions an unscaled terrain anchor (compatibility overload). */
    void setTerrainPlacement(const QVector2D &offsetXZ, float aircraftAltitudeScene) noexcept;

    bool initialize(QString *errorMessage = nullptr);
    bool draw(const QMatrix4x4 &view, const QMatrix4x4 &projection,
              const QQuaternion &aircraftAttitude, QString *errorMessage = nullptr);
    void cleanup() noexcept;

    [[nodiscard]] bool ready() const noexcept {
        return m_ready;
    }

  private:
    struct DrawRange final {
        std::size_t firstIndex = 0U;
        std::size_t indexCount = 0U;
        QVector4D color;
        int textureIndex = -1;
        GLenum primitive = GL_TRIANGLES;
    };

    bool compilePrograms(QString *errorMessage);
    bool createStaticGeometry(QString *errorMessage);
    bool uploadModel(QString *errorMessage);
    bool uploadSkybox(QString *errorMessage);
    bool uploadInterleaved(const std::vector<float> &vertices,
                           const std::vector<std::uint32_t> &indices, QOpenGLBuffer &vertexBuffer,
                           QOpenGLBuffer &indexBuffer, QOpenGLVertexArrayObject &vertexArray,
                           QString *errorMessage);
    void drawMesh(QOpenGLVertexArrayObject &vertexArray, QOpenGLBuffer &indexBuffer,
                  const QMatrix4x4 &model, const QMatrix4x4 &view, const QMatrix4x4 &projection,
                  const DrawRange &range, bool unlit = false, float pointSize = 1.0F);
    void drawSkybox(const QMatrix4x4 &view, const QMatrix4x4 &projection);
    void drawAircraft(const QMatrix4x4 &view, const QMatrix4x4 &projection,
                      const QQuaternion &attitude);

    std::unique_ptr<QOpenGLShaderProgram> m_meshProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_skyProgram;
    std::unique_ptr<QOpenGLTexture> m_cubemap;
    std::unique_ptr<QOpenGLTexture> m_whiteModelTexture;
    std::vector<std::unique_ptr<QOpenGLTexture>> m_modelTextures;
    PlaneSurfaceGpuLayer m_surfaceLayer;
    PlaneTerrainGpuLayer m_terrainLayer;

    QOpenGLBuffer m_modelVertices;
    QOpenGLBuffer m_modelIndices{QOpenGLBuffer::IndexBuffer};
    QOpenGLVertexArrayObject m_modelArray;
    QOpenGLBuffer m_skyVertices;
    QOpenGLVertexArrayObject m_skyArray;

    std::shared_ptr<const PlaneModelMesh> m_model;
    PlaneSurfaceState m_surfaceState;
    QVector2D m_terrainOffsetXZ;
    QVector2D m_terrainScaleXZ{1.0F, 1.0F};
    float m_aircraftAltitudeScene = 0.0F;
    CubemapFaces m_pendingSkybox;
    bool m_modelPending = false;
    bool m_skyboxPending = false;
    bool m_surfaceVisible = false;
    bool m_terrainVisible = false;
    bool m_ready = false;
};
