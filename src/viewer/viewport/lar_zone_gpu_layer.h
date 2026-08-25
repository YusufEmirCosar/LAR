#pragma once

/**
 * @file lar_zone_gpu_layer.h
 * @brief OpenGL upload and draw layer for dynamic LAR zone geometry.
 */

#include "viewer/map/map_camera.h"
#include "viewer/viewport/lar_zone_mesh.h"

#include <QMatrix4x4>
#include <QObject>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QPointF>
#include <QVector2D>

class QOpenGLShaderProgram;

/** @brief Camera uniforms and periodic-copy range for one zone draw. */
struct LarZoneRenderState final {
    bool sphere = false;
    lar::map::SphereProjectionParameters sphereProjection;
    QPointF sphereCenterDegrees;
    float bearingRadians = 0.0F;
    QPointF flatCenter;
    QMatrix4x4 projection;
    lar::map::WorldCopyRange worldCopies{0, 0};
};

/** @brief Owns GPU resources for filled and outlined LAR zone ranges. */
class LarZoneGpuLayer final : public QObject, protected QOpenGLFunctions {
    Q_OBJECT

  public:
    explicit LarZoneGpuLayer(QObject *parent = nullptr);
    ~LarZoneGpuLayer() override;

    bool initialize();
    void cleanup();
    bool upload(const LarZoneMesh &mesh);
    void draw(const LarZoneRenderState &state);

  signals:
    void diagnosticRaised(const QString &message);

  private:
    bool compileProgram();
    bool ensureBuffers();
    bool writeBuffer(QOpenGLBuffer &buffer, int &capacityBytes, const void *data,
                     std::size_t sizeBytes, const QString &description);
    void drawRange(const LarZoneDrawRange &range, unsigned int primitive, const QColor &color,
                   const lar::map::WorldCopyRange &copies);

    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLBuffer m_vertexBuffer;
    QOpenGLBuffer m_indexBuffer{QOpenGLBuffer::IndexBuffer};
    QOpenGLVertexArrayObject m_vertexArray;
    LarZoneDrawRange m_inRangeFill;
    LarZoneDrawRange m_inZoneFill;
    LarZoneDrawRange m_inRangeLines;
    LarZoneDrawRange m_inZoneLines;
    LarZoneCoordinateSpace m_coordinateSpace = LarZoneCoordinateSpace::GeographicDegrees;
    QPointF m_coordinateOrigin;
    std::size_t m_indexCount = 0U;
    int m_vertexCapacityBytes = 0;
    int m_indexCapacityBytes = 0;

    int m_positionAttribute = -1;
    int m_sphereModeUniform = -1;
    int m_worldOffsetUniform = -1;
    int m_sphereLatitudeSinCosUniform = -1;
    int m_cameraBearingUniform = -1;
    int m_positionOriginDeltaUniform = -1;
    int m_projectionUniform = -1;
    int m_colorUniform = -1;
};
