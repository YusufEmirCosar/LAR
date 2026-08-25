#pragma once

/**
 * @file lar_parametric_zone_gpu_layer.h
 * @brief Reusable-topology GPU path for regular LAR zones.
 */

#include "domain/state.h"
#include "viewer/map/map_camera.h"
#include "viewer/viewport/lar_zone_gpu_layer.h"

#include <QBitArray>
#include <QColor>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QVector2D>

#include <cstddef>

class QOpenGLShaderProgram;

/** @brief GPU-generated geodesic parameters for one regular LAR zone. */
struct LarParametricZoneState final {
    bool valid = false;
    QVector2D centerRadians;
    QVector2D radiiMeters;
    QVector2D startAndSpanRadians;
    bool fullCircle = false;
};

/**
 * @brief Draws regular IR/IZ zones from compact parameters and static LOD.
 *
 * setZones() proves fixed topology and float-coordinate error against the
 * 0.65-pixel error contract. Pole-clipped, extreme-zoom, or numerically
 * sensitive zones are rejected and remain on the revisioned CPU mesh path.
 */
class LarParametricZoneGpuLayer final : public QObject, protected QOpenGLFunctions {
    Q_OBJECT

  public:
    explicit LarParametricZoneGpuLayer(QObject *parent = nullptr);
    ~LarParametricZoneGpuLayer() override;

    bool setZones(const Target &target, const QBitArray &availableFields,
                  const lar::map::MapCamera &camera, int viewportWidth, int viewportHeight);
    bool isEligible() const noexcept {
        return m_eligible;
    }
    bool hasZones() const noexcept {
        return m_hasZones;
    }
    bool longitudeBounds(double *minimum, double *maximum) const noexcept;

    bool initialize();
    void cleanup();
    void draw(const LarZoneRenderState &state);

  signals:
    void diagnosticRaised(const QString &message);

  private:
    bool compileProgram();
    bool buildTopology();
    void drawRange(const LarParametricZoneState &zone, const LarZoneDrawRange &range,
                   unsigned int primitive, const QColor &color,
                   const lar::map::WorldCopyRange &copies);

    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLBuffer m_vertexBuffer;
    QOpenGLBuffer m_indexBuffer{QOpenGLBuffer::IndexBuffer};
    QOpenGLVertexArrayObject m_vertexArray;
    LarZoneDrawRange m_inRangeFill;
    LarZoneDrawRange m_inRangeLines;
    LarZoneDrawRange m_inZoneFill;
    LarZoneDrawRange m_inZoneLines;
    LarZoneDrawRange m_inZoneFullLines;
    std::size_t m_indexCount = 0U;
    LarParametricZoneState m_inRange;
    LarParametricZoneState m_inZone;
    bool m_eligible = false;
    bool m_hasZones = false;
    bool m_initialized = false;
    double m_minimumLongitude = 0.0;
    double m_maximumLongitude = 0.0;

    int m_positionAttribute = -1;
    int m_sphereModeUniform = -1;
    int m_worldOffsetUniform = -1;
    int m_sphereCenterHighUniform = -1;
    int m_sphereCenterLowUniform = -1;
    int m_sphereLatitudeSinCosUniform = -1;
    int m_cameraBearingUniform = -1;
    int m_flatCenterUniform = -1;
    int m_projectionUniform = -1;
    int m_zoneCenterUniform = -1;
    int m_zoneRadiiUniform = -1;
    int m_startSpanUniform = -1;
    int m_colorUniform = -1;
};
