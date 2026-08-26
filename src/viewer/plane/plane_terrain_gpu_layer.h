#pragma once

/**
 * @file plane_terrain_gpu_layer.h
 * @brief Context-owned upload and draw pass for immutable DTED terrain patches.
 */

#include "viewer/plane/plane_terrain_patch.h"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QVector2D>

#include <memory>
#include <optional>

class QOpenGLShaderProgram;

/** @brief Uploads terrain only from an active paint context and retains CPU data across GL loss. */
class PlaneTerrainGpuLayer final : protected QOpenGLFunctions {
  public:
    PlaneTerrainGpuLayer();
    ~PlaneTerrainGpuLayer();
    PlaneTerrainGpuLayer(const PlaneTerrainGpuLayer &) = delete;
    PlaneTerrainGpuLayer &operator=(const PlaneTerrainGpuLayer &) = delete;

    void setPatch(PlaneTerrainPatchPtr patch) noexcept;
    [[nodiscard]] bool initialize(QString *errorMessage = nullptr);
    [[nodiscard]] bool draw(const QMatrix4x4 &view, const QMatrix4x4 &projection,
                            const QVector2D &offsetXZ, const QVector2D &scaleXZ,
                            float aircraftAltitudeScene,
                            QString *errorMessage = nullptr);
    /** @brief Draws an unscaled terrain anchor (compatibility overload). */
    [[nodiscard]] bool draw(const QMatrix4x4 &view, const QMatrix4x4 &projection,
                            const QVector2D &offsetXZ, float aircraftAltitudeScene,
                            QString *errorMessage = nullptr);
    void cleanup() noexcept;

    [[nodiscard]] bool hasPatch() const noexcept {
        return m_patch != nullptr && !m_patch->empty();
    }
    [[nodiscard]] bool hasRenderableGeometry() const noexcept {
        return hasPatch() && !m_uploadPending && m_indexCount > 0;
    }
    [[nodiscard]] std::optional<float>
    centerGroundHeight(float aircraftAltitudeScene) const noexcept;

  private:
    [[nodiscard]] bool compileProgram(QString *errorMessage);
    [[nodiscard]] bool createBuffers(QString *errorMessage);
    [[nodiscard]] bool uploadPending(QString *errorMessage);

    std::unique_ptr<QOpenGLShaderProgram> m_program;
    QOpenGLBuffer m_vertices;
    QOpenGLBuffer m_indices{QOpenGLBuffer::IndexBuffer};
    QOpenGLVertexArrayObject m_array;
    PlaneTerrainPatchPtr m_patch;
    int m_indexCount = 0;
    bool m_uploadPending = false;
    bool m_ready = false;
};
