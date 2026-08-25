#pragma once

/**
 * @file plane_surface_gpu_layer.h
 * @brief Context-owned flat ground, metric grid, target, and LAR renderer.
 */

#include "viewer/plane/plane_surface_state.h"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QVector4D>

#include <cstddef>
#include <memory>

class QOpenGLShaderProgram;

/** @brief Renders the optional local tactical surface beneath the aircraft. */
class PlaneSurfaceGpuLayer final : protected QOpenGLFunctions {
  public:
    PlaneSurfaceGpuLayer();
    ~PlaneSurfaceGpuLayer();
    PlaneSurfaceGpuLayer(const PlaneSurfaceGpuLayer &other) = delete;
    PlaneSurfaceGpuLayer &operator=(const PlaneSurfaceGpuLayer &other) = delete;

    bool initialize(QString *errorMessage = nullptr);
    void draw(const PlaneSurfaceState &state, const QMatrix4x4 &view, const QMatrix4x4 &projection,
              bool drawGround = true, bool drawOverlays = true);
    void cleanup() noexcept;

  private:
    struct Range final {
        std::size_t first = 0U;
        std::size_t count = 0U;
    };

    bool compilePrograms(QString *errorMessage);
    bool createShapeGeometry(QString *errorMessage);
    bool createZoneGeometry(QString *errorMessage);
    void drawShape(const Range &range, unsigned int primitive, const QMatrix4x4 &view,
                   const QMatrix4x4 &projection, float horizontalScale, const QVector2D &offsetXZ,
                   float groundHeight, float surfaceHalfExtent, const QVector4D &color,
                   bool fadeAtEdge, float verticalScale = 1.0F);
    void drawZone(const PlaneSurfaceZone &zone, const Range &range, unsigned int primitive,
                  const QMatrix4x4 &view, const QMatrix4x4 &projection, float groundHeight,
                  const QVector4D &color);

    std::unique_ptr<QOpenGLShaderProgram> m_shapeProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_zoneProgram;
    QOpenGLBuffer m_shapeVertices;
    QOpenGLVertexArrayObject m_shapeArray;
    QOpenGLBuffer m_zoneVertices;
    QOpenGLBuffer m_zoneIndices{QOpenGLBuffer::IndexBuffer};
    QOpenGLVertexArrayObject m_zoneArray;
    Range m_ground;
    Range m_minorGrid;
    Range m_majorGrid;
    Range m_gridAxes;
    Range m_targetPyramid;
    Range m_zoneFill;
    Range m_circleLines;
    Range m_annulusLines;
    Range m_sectorLines;
    bool m_ready = false;
};
