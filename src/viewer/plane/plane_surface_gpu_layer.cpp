
#include "viewer/plane/plane_surface_gpu_layer.h"

#include "viewer/plane/plane_surface_shaders.h"

#include <QOpenGLShaderProgram>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

constexpr int GridHalfLineCount = PlaneSurfaceGridHalfLineCount;
constexpr int AngularSegments = 128;

void setError(QString *destination, const QString &message) {
    if (destination != nullptr) {
        *destination = message;
    }
}

bool compileProgram(QOpenGLShaderProgram &program, const char *legacyVertex,
                    const char *legacyFragment, const char *coreVertex, const char *coreFragment,
                    const char *attribute, QString *errorMessage) {
    bool vertex = program.addShaderFromSourceCode(QOpenGLShader::Vertex, legacyVertex);
    bool fragment = program.addShaderFromSourceCode(QOpenGLShader::Fragment, legacyFragment);
    program.bindAttributeLocation(attribute, 0);
    bool linked = program.link();
    if (!vertex || !fragment || !linked) {
        program.removeAllShaders();
        vertex = program.addShaderFromSourceCode(QOpenGLShader::Vertex, coreVertex);
        fragment = program.addShaderFromSourceCode(QOpenGLShader::Fragment, coreFragment);
        program.bindAttributeLocation(attribute, 0);
        linked = program.link();
    }
    if (!vertex || !fragment || !linked) {
        setError(
            errorMessage,
            QStringLiteral("Plane surface shader initialization failed: %1").arg(program.log()));
        return false;
    }
    return true;
}

void vertex(std::vector<float> &vertices, float x, float y, float z) {
    vertices.insert(vertices.end(), {x, y, z});
}

void triangle(std::vector<float> &vertices, const QVector3D &a, const QVector3D &b,
              const QVector3D &c) {
    vertex(vertices, a.x(), a.y(), a.z());
    vertex(vertices, b.x(), b.y(), b.z());
    vertex(vertices, c.x(), c.y(), c.z());
}

void gridLine(std::vector<float> &vertices, int coordinate, bool alongZ) {
    const float value = static_cast<float>(coordinate);
    const float edge = static_cast<float>(GridHalfLineCount);
    if (alongZ) {
        vertex(vertices, value, 0.0F, -edge);
        vertex(vertices, value, 0.0F, edge);
    } else {
        vertex(vertices, -edge, 0.0F, value);
        vertex(vertices, edge, 0.0F, value);
    }
}

bool validZone(const PlaneSurfaceZone &zone) noexcept {
    constexpr float TwoPi = 6.28318530717958647692F;
    return zone.visible && std::isfinite(zone.centerXZ.x()) && std::isfinite(zone.centerXZ.y()) &&
           std::isfinite(zone.innerRadius) && std::isfinite(zone.outerRadius) &&
           std::isfinite(zone.startBearingRadians) && std::isfinite(zone.spanRadians) &&
           zone.innerRadius >= 0.0F && zone.outerRadius >= zone.innerRadius &&
           zone.outerRadius <= PlaneSurfaceMaximumCoordinate && zone.spanRadians > 0.0F &&
           zone.spanRadians <= TwoPi + 0.0001F;
}

} // namespace

PlaneSurfaceGpuLayer::PlaneSurfaceGpuLayer() = default;

PlaneSurfaceGpuLayer::~PlaneSurfaceGpuLayer() = default;

bool PlaneSurfaceGpuLayer::compilePrograms(QString *errorMessage) {
    m_shapeProgram = std::make_unique<QOpenGLShaderProgram>();
    m_zoneProgram = std::make_unique<QOpenGLShaderProgram>();
    return compileProgram(*m_shapeProgram, plane::surface::shaders::ShapeVertexLegacy,
                          plane::surface::shaders::ShapeFragmentLegacy,
                          plane::surface::shaders::ShapeVertexCore,
                          plane::surface::shaders::ShapeFragmentCore, "aPosition", errorMessage) &&
           compileProgram(*m_zoneProgram, plane::surface::shaders::ZoneVertexLegacy,
                          plane::surface::shaders::ZoneFragmentLegacy,
                          plane::surface::shaders::ZoneVertexCore,
                          plane::surface::shaders::ZoneFragmentCore, "aParam", errorMessage);
}

bool PlaneSurfaceGpuLayer::createShapeGeometry(QString *errorMessage) {
    std::vector<float> vertices;
    vertices.reserve(8000U);
    const auto capture = [&vertices](std::size_t firstVertex) {
        return Range{firstVertex, vertices.size() / 3U - firstVertex};
    };
    std::size_t first = vertices.size() / 3U;
    triangle(vertices, {-1.0F, 0.0F, -1.0F}, {1.0F, 0.0F, -1.0F}, {1.0F, 0.0F, 1.0F});
    triangle(vertices, {-1.0F, 0.0F, -1.0F}, {1.0F, 0.0F, 1.0F}, {-1.0F, 0.0F, 1.0F});
    m_ground = capture(first);

    first = vertices.size() / 3U;
    for (int coordinate = -GridHalfLineCount; coordinate <= GridHalfLineCount; ++coordinate) {
        if (coordinate % 5 != 0) {
            gridLine(vertices, coordinate, true);
            gridLine(vertices, coordinate, false);
        }
    }
    m_minorGrid = capture(first);
    first = vertices.size() / 3U;
    for (int coordinate = -GridHalfLineCount; coordinate <= GridHalfLineCount; ++coordinate) {
        // Zero is the phase-wrapped major line nearest the aircraft, not the highlighted
        // Earth origin. The Earth-fixed axes are drawn separately with groundOriginXZ.
        if (coordinate % 5 == 0) {
            gridLine(vertices, coordinate, true);
            gridLine(vertices, coordinate, false);
        }
    }
    m_majorGrid = capture(first);
    first = vertices.size() / 3U;
    gridLine(vertices, 0, true);
    gridLine(vertices, 0, false);
    m_gridAxes = capture(first);

    first = vertices.size() / 3U;
    constexpr QVector3D PyramidApex{0.0F, 1.5F, 0.0F};
    constexpr QVector3D PyramidNorthWest{-1.0F, 0.0F, -1.0F};
    constexpr QVector3D PyramidNorthEast{1.0F, 0.0F, -1.0F};
    constexpr QVector3D PyramidSouthEast{1.0F, 0.0F, 1.0F};
    constexpr QVector3D PyramidSouthWest{-1.0F, 0.0F, 1.0F};
    triangle(vertices, PyramidApex, PyramidNorthWest, PyramidNorthEast);
    triangle(vertices, PyramidApex, PyramidNorthEast, PyramidSouthEast);
    triangle(vertices, PyramidApex, PyramidSouthEast, PyramidSouthWest);
    triangle(vertices, PyramidApex, PyramidSouthWest, PyramidNorthWest);
    m_targetPyramid = capture(first);

    if (!m_shapeVertices.create() || !m_shapeArray.create() ||
        vertices.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) / sizeof(float)) {
        setError(errorMessage, QStringLiteral("Plane surface geometry could not be created."));
        return false;
    }
    m_shapeProgram->bind();
    QOpenGLVertexArrayObject::Binder binder(&m_shapeArray);
    m_shapeVertices.bind();
    m_shapeVertices.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_shapeVertices.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(float)));
    m_shapeProgram->enableAttributeArray(0);
    m_shapeProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * static_cast<int>(sizeof(float)));
    m_shapeProgram->release();
    return true;
}

bool PlaneSurfaceGpuLayer::createZoneGeometry(QString *errorMessage) {
    std::vector<float> vertices;
    vertices.reserve(static_cast<std::size_t>((AngularSegments + 1) * 4));
    for (int row = 0; row <= 1; ++row) {
        for (int column = 0; column <= AngularSegments; ++column) {
            vertices.push_back(static_cast<float>(row));
            vertices.push_back(static_cast<float>(column) / static_cast<float>(AngularSegments));
        }
    }
    std::vector<std::uint32_t> indices;
    indices.reserve(2048U);
    auto capture = [&indices](std::size_t first) { return Range{first, indices.size() - first}; };
    std::size_t first = indices.size();
    const std::uint32_t outerStart = static_cast<std::uint32_t>(AngularSegments + 1);
    for (int column = 0; column < AngularSegments; ++column) {
        const std::uint32_t inner = static_cast<std::uint32_t>(column);
        const std::uint32_t outer = outerStart + inner;
        indices.insert(indices.end(), {inner, outer, inner + 1U, outer, outer + 1U, inner + 1U});
    }
    m_zoneFill = capture(first);
    const auto appendArc = [&indices](std::uint32_t start) {
        for (int column = 0; column < AngularSegments; ++column) {
            const std::uint32_t current = start + static_cast<std::uint32_t>(column);
            indices.insert(indices.end(), {current, current + 1U});
        }
    };
    first = indices.size();
    appendArc(outerStart);
    m_circleLines = capture(first);
    first = indices.size();
    appendArc(outerStart);
    appendArc(0U);
    m_annulusLines = capture(first);
    first = indices.size();
    appendArc(outerStart);
    appendArc(0U);
    indices.insert(indices.end(), {0U, outerStart, static_cast<std::uint32_t>(AngularSegments),
                                   outerStart + static_cast<std::uint32_t>(AngularSegments)});
    m_sectorLines = capture(first);

    const bool tooLarge =
        vertices.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) / sizeof(float) ||
        indices.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) / sizeof(std::uint32_t);
    if (tooLarge || !m_zoneVertices.create() || !m_zoneIndices.create() || !m_zoneArray.create()) {
        setError(errorMessage, QStringLiteral("Plane LAR geometry could not be created."));
        return false;
    }
    m_zoneProgram->bind();
    QOpenGLVertexArrayObject::Binder binder(&m_zoneArray);
    m_zoneVertices.bind();
    m_zoneVertices.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_zoneVertices.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(float)));
    m_zoneIndices.bind();
    m_zoneIndices.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_zoneIndices.allocate(indices.data(),
                           static_cast<int>(indices.size() * sizeof(std::uint32_t)));
    m_zoneProgram->enableAttributeArray(0);
    m_zoneProgram->setAttributeBuffer(0, GL_FLOAT, 0, 2, 2 * static_cast<int>(sizeof(float)));
    m_zoneProgram->release();
    return true;
}

bool PlaneSurfaceGpuLayer::initialize(QString *errorMessage) {
    initializeOpenGLFunctions();
    cleanup();
    m_ready = compilePrograms(errorMessage) && createShapeGeometry(errorMessage) &&
              createZoneGeometry(errorMessage);
    return m_ready;
}

void PlaneSurfaceGpuLayer::drawShape(const Range &range, unsigned int primitive,
                                     const QMatrix4x4 &view, const QMatrix4x4 &projection,
                                     float horizontalScale, const QVector2D &offsetXZ,
                                     float groundHeight, float surfaceHalfExtent,
                                     const QVector4D &color, bool fadeAtEdge, float verticalScale) {
    if (range.count == 0U ||
        range.count > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        return;
    }
    m_shapeProgram->bind();
    m_shapeProgram->setUniformValue("uView", view);
    m_shapeProgram->setUniformValue("uProjection", projection);
    m_shapeProgram->setUniformValue("uOffsetXZ", offsetXZ);
    m_shapeProgram->setUniformValue("uHorizontalScale", horizontalScale);
    m_shapeProgram->setUniformValue("uVerticalScale", verticalScale);
    m_shapeProgram->setUniformValue("uGroundHeight", groundHeight);
    m_shapeProgram->setUniformValue("uSurfaceHalfExtent", surfaceHalfExtent);
    m_shapeProgram->setUniformValue("uColor", color);
    m_shapeProgram->setUniformValue("uFadeAtEdge", fadeAtEdge);
    QOpenGLVertexArrayObject::Binder binder(&m_shapeArray);
    glDrawArrays(primitive, static_cast<GLint>(range.first), static_cast<GLsizei>(range.count));
    m_shapeProgram->release();
}

void PlaneSurfaceGpuLayer::drawZone(const PlaneSurfaceZone &zone, const Range &range,
                                    unsigned int primitive, const QMatrix4x4 &view,
                                    const QMatrix4x4 &projection, float groundHeight,
                                    const QVector4D &color) {
    if (!validZone(zone) || range.count == 0U ||
        range.count > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        return;
    }
    m_zoneProgram->bind();
    m_zoneProgram->setUniformValue("uView", view);
    m_zoneProgram->setUniformValue("uProjection", projection);
    m_zoneProgram->setUniformValue("uCenterXZ", zone.centerXZ);
    m_zoneProgram->setUniformValue("uRadii", QVector2D(zone.innerRadius, zone.outerRadius));
    m_zoneProgram->setUniformValue("uStartSpan",
                                   QVector2D(zone.startBearingRadians, zone.spanRadians));
    m_zoneProgram->setUniformValue("uGroundHeight", groundHeight);
    m_zoneProgram->setUniformValue("uColor", color);
    QOpenGLVertexArrayObject::Binder binder(&m_zoneArray);
    m_zoneIndices.bind();
    glDrawElements(primitive, static_cast<GLsizei>(range.count), GL_UNSIGNED_INT,
                   reinterpret_cast<const void *>(range.first * sizeof(std::uint32_t)));
    m_zoneProgram->release();
}

void PlaneSurfaceGpuLayer::draw(const PlaneSurfaceState &state, const QMatrix4x4 &view,
                                const QMatrix4x4 &projection, bool drawGround, bool drawOverlays) {
    if (!m_ready || (!drawGround && !drawOverlays)) {
        return;
    }
    const float halfExtent =
        std::clamp(state.surfaceHalfExtent, 64.0F, PlaneSurfaceMaximumCoordinate);
    const float maximumGridSpacing =
        PlaneSurfaceMaximumCoordinate / static_cast<float>(GridHalfLineCount);
    const float gridSpacing = std::clamp(state.gridSpacingSceneUnits, 0.25F, maximumGridSpacing);
    if (drawGround) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        drawShape(m_ground, GL_TRIANGLES, view, projection, halfExtent, {}, state.surfaceHeight,
                  halfExtent, {0.20F, 0.29F, 0.25F, 1.0F}, false);
    }
    if (!drawOverlays) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    drawShape(m_minorGrid, GL_LINES, view, projection, gridSpacing, state.gridPhaseXZ,
              state.surfaceHeight + 0.015F, halfExtent, {0.64F, 0.72F, 0.68F, 0.34F}, true);
    drawShape(m_majorGrid, GL_LINES, view, projection, gridSpacing, state.gridPhaseXZ,
              state.surfaceHeight + 0.020F, halfExtent, {0.75F, 0.82F, 0.79F, 0.58F}, true);
    drawShape(m_gridAxes, GL_LINES, view, projection, gridSpacing, state.groundOriginXZ,
              state.surfaceHeight + 0.025F, halfExtent, {0.88F, 0.92F, 0.90F, 0.80F}, true);
    drawZone(state.inRange, m_zoneFill, GL_TRIANGLES, view, projection,
             state.surfaceHeight + 0.035F, {0.20F, 0.43F, 0.57F, 0.35F});
    drawZone(state.inZone, m_zoneFill, GL_TRIANGLES, view, projection, state.surfaceHeight + 0.045F,
             {0.08F, 0.51F, 0.40F, 0.46F});

    glLineWidth(2.0F);
    drawZone(state.inRange, m_circleLines, GL_LINES, view, projection, state.surfaceHeight + 0.055F,
             {0.35F, 0.70F, 0.91F, 1.0F});
    const Range &inZoneLines = state.inZone.fullCircle ? m_annulusLines : m_sectorLines;
    drawZone(state.inZone, inZoneLines, GL_LINES, view, projection, state.surfaceHeight + 0.060F,
             {0.16F, 0.82F, 0.64F, 1.0F});
    if (state.targetVisible && std::isfinite(state.targetXZ.x()) &&
        std::isfinite(state.targetXZ.y())) {
        const float targetScale =
            std::isfinite(state.targetMarkerScale)
                ? std::clamp(state.targetMarkerScale, 1.0F, PlaneSurfaceMaximumCoordinate)
                : 1.0F;
        drawShape(m_targetPyramid, GL_TRIANGLES, view, projection, targetScale, state.targetXZ,
                  state.surfaceHeight + 0.070F, halfExtent, {0.90F, 0.20F, 0.14F, 1.0F}, false,
                  targetScale);
    }
    glLineWidth(1.0F);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void PlaneSurfaceGpuLayer::cleanup() noexcept {
    m_ready = false;
    for (QOpenGLBuffer *buffer : {&m_shapeVertices, &m_zoneVertices, &m_zoneIndices}) {
        if (buffer->isCreated()) {
            buffer->destroy();
        }
    }
    for (QOpenGLVertexArrayObject *array : {&m_shapeArray, &m_zoneArray}) {
        if (array->isCreated()) {
            array->destroy();
        }
    }
    m_shapeProgram.reset();
    m_zoneProgram.reset();
}
