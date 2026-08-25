
#include "viewer/viewport/lar_zone_gpu_layer.h"

#include "viewer/map/map_asset_limits.h"
#include "viewer/viewport/lar_zone_mesh_builder.h"

#include <QColor>
#include <QOpenGLShaderProgram>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

constexpr int VertexStrideFloats = 3;
constexpr int VertexStrideBytes = VertexStrideFloats * static_cast<int>(sizeof(float));

const char *vertexShaderLegacy = R"(
    attribute vec3 aPos;
    uniform bool uSphereMode;
    uniform float uWorldOffsetX;
    uniform vec2 uSphereLatitudeSinCos;
    uniform float uCameraBearing;
    uniform vec2 uPositionOriginDelta;
    uniform mat4 uProjection;
    varying float vZ;

    vec2 rotateForCamera(vec2 value) {
        float cosine = cos(uCameraBearing);
        float sine = sin(uCameraBearing);
        return vec2(
            value.x * cosine - value.y * sine,
            value.x * sine + value.y * cosine
        );
    }

    vec3 projectSphereDelta(vec2 deltaDegrees) {
        float degreesToRadians = 3.14159265358979323846 / 180.0;
        vec2 delta = deltaDegrees * degreesToRadians;
        float sinLongitude = sin(delta.x);
        float cosLongitude = cos(delta.x);
        float sinLatitude = sin(delta.y);
        float cosLatitude = cos(delta.y);
        float centerSin = uSphereLatitudeSinCos.x;
        float centerCos = uSphereLatitudeSinCos.y;
        float oneMinusCosLongitude = 1.0 - cosLongitude;
        float x = (centerCos * cosLatitude - centerSin * sinLatitude) * sinLongitude;
        float y = centerSin * centerCos * cosLatitude * oneMinusCosLongitude
                  + (centerCos * centerCos + centerSin * centerSin * cosLongitude) * sinLatitude;
        float z = (centerSin * centerSin + centerCos * centerCos * cosLongitude) * cosLatitude
                  + centerSin * centerCos * oneMinusCosLongitude * sinLatitude;
        return vec3(x, y, z);
    }

    void main() {
        if (!uSphereMode) {
            vZ = 1.0;
            vec2 mapDelta = vec2(aPos.x + uWorldOffsetX, aPos.z)
                            + uPositionOriginDelta;
            vec2 viewPosition = rotateForCamera(mapDelta);
            gl_Position = uProjection * vec4(viewPosition, 0.0, 1.0);
        } else {
            vec3 spherePosition = projectSphereDelta(aPos.xy + uPositionOriginDelta);
            vZ = spherePosition.z;
            gl_Position = uProjection
                          * vec4(rotateForCamera(spherePosition.xy), 0.0, 1.0);
        }
    }
)";

const char *fragmentShaderLegacy = R"(
    uniform vec4 uColor;
    varying float vZ;

    void main() {
        if (vZ < 0.0) {
            discard;
        }
        gl_FragColor = uColor;
    }
)";

const char *vertexShaderCore = R"(#version 150
in vec3 aPos;
uniform bool uSphereMode;
uniform float uWorldOffsetX;
uniform vec2 uSphereLatitudeSinCos;
uniform float uCameraBearing;
uniform vec2 uPositionOriginDelta;
uniform mat4 uProjection;
out float vZ;

vec2 rotateForCamera(vec2 value) {
    float cosine = cos(uCameraBearing);
    float sine = sin(uCameraBearing);
    return vec2(
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine
    );
}

vec3 projectSphereDelta(vec2 deltaDegrees) {
    float degreesToRadians = 3.14159265358979323846 / 180.0;
    vec2 delta = deltaDegrees * degreesToRadians;
    float sinLongitude = sin(delta.x);
    float cosLongitude = cos(delta.x);
    float sinLatitude = sin(delta.y);
    float cosLatitude = cos(delta.y);
    float centerSin = uSphereLatitudeSinCos.x;
    float centerCos = uSphereLatitudeSinCos.y;
    float oneMinusCosLongitude = 1.0 - cosLongitude;
    float x = (centerCos * cosLatitude - centerSin * sinLatitude) * sinLongitude;
    float y = centerSin * centerCos * cosLatitude * oneMinusCosLongitude
              + (centerCos * centerCos + centerSin * centerSin * cosLongitude) * sinLatitude;
    float z = (centerSin * centerSin + centerCos * centerCos * cosLongitude) * cosLatitude
              + centerSin * centerCos * oneMinusCosLongitude * sinLatitude;
    return vec3(x, y, z);
}

void main() {
    if (!uSphereMode) {
        vZ = 1.0;
        vec2 mapDelta = vec2(aPos.x + uWorldOffsetX, aPos.z)
                        + uPositionOriginDelta;
        vec2 viewPosition = rotateForCamera(mapDelta);
        gl_Position = uProjection * vec4(viewPosition, 0.0, 1.0);
    } else {
        vec3 spherePosition = projectSphereDelta(aPos.xy + uPositionOriginDelta);
        vZ = spherePosition.z;
        gl_Position = uProjection * vec4(rotateForCamera(spherePosition.xy), 0.0, 1.0);
    }
}
)";

const char *fragmentShaderCore = R"(#version 150
uniform vec4 uColor;
in float vZ;
out vec4 fragColor;

void main() {
    if (vZ < 0.0) {
        discard;
    }
    fragColor = uColor;
}
)";

QVector4D colorVector(const QColor &color) {
    return {static_cast<float>(color.redF()), static_cast<float>(color.greenF()),
            static_cast<float>(color.blueF()), static_cast<float>(color.alphaF())};
}

int retainedCapacity(int requiredBytes) {
    int capacity = 256;
    while (capacity < requiredBytes && capacity <= std::numeric_limits<int>::max() / 2) {
        capacity *= 2;
    }
    return std::max(capacity, requiredBytes);
}

bool rangeIsValid(const LarZoneDrawRange &range, std::size_t indexCount,
                  std::size_t primitiveSize) noexcept {
    return range.firstIndex <= indexCount && range.indexCount <= indexCount - range.firstIndex &&
           range.indexCount % primitiveSize == 0U;
}

bool meshIsValid(const LarZoneMesh &mesh) noexcept {
    if (mesh.vertices.size() % 3U != 0U ||
        mesh.vertices.size() / 3U > LarZoneMeshBuilder::MaximumVertexCount ||
        mesh.indices.size() > LarZoneMeshBuilder::MaximumIndexCount) {

        return false;
    }
    if (!std::isfinite(mesh.coordinateOrigin.x()) || !std::isfinite(mesh.coordinateOrigin.y())) {
        return false;
    }
    const double longitudeLimit = mesh.coordinateSpace == LarZoneCoordinateSpace::GeographicDegrees
                                      ? lar::map::limits::MaximumAbsoluteLongitude
                                      : lar::map::limits::MaximumAbsoluteLongitude + 180.0;
    const double latitudeLimit =
        mesh.coordinateSpace == LarZoneCoordinateSpace::SphereCameraRelative
            ? 180.0
            : lar::map::limits::MaximumAbsoluteLatitude;
    const double mercatorLimit =
        mesh.coordinateSpace == LarZoneCoordinateSpace::MercatorCameraRelative
            ? lar::map::limits::MaximumAbsoluteMercatorY * 2.0
            : lar::map::limits::MaximumAbsoluteMercatorY;
    for (std::size_t offset = 0U; offset + 2U < mesh.vertices.size(); offset += 3U) {
        if (!std::isfinite(mesh.vertices[offset]) || !std::isfinite(mesh.vertices[offset + 1U]) ||
            !std::isfinite(mesh.vertices[offset + 2U]) ||
            std::abs(mesh.vertices[offset]) > longitudeLimit ||
            std::abs(mesh.vertices[offset + 1U]) > latitudeLimit ||
            std::abs(mesh.vertices[offset + 2U]) > mercatorLimit) {
            return false;
        }
    }
    const std::size_t vertexCount = mesh.vertices.size() / 3U;
    if (std::any_of(mesh.indices.begin(), mesh.indices.end(), [vertexCount](std::uint32_t index) {
            return static_cast<std::size_t>(index) >= vertexCount;
        })) {

        return false;
    }
    return rangeIsValid(mesh.inRangeFill, mesh.indices.size(), 3U) &&
           rangeIsValid(mesh.inZoneFill, mesh.indices.size(), 3U) &&
           rangeIsValid(mesh.inRangeLines, mesh.indices.size(), 2U) &&
           rangeIsValid(mesh.inZoneLines, mesh.indices.size(), 2U);
}

} // namespace

LarZoneGpuLayer::LarZoneGpuLayer(QObject *parent) : QObject(parent) {}

LarZoneGpuLayer::~LarZoneGpuLayer() {
    delete m_program;
}

bool LarZoneGpuLayer::initialize() {
    initializeOpenGLFunctions();
    cleanup();
    if (!compileProgram() || !ensureBuffers()) {
        return false;
    }
    return true;
}

void LarZoneGpuLayer::cleanup() {
    m_vertexArray.destroy();
    m_vertexBuffer.destroy();
    m_indexBuffer.destroy();
    delete m_program;
    m_program = nullptr;
    m_vertexCapacityBytes = 0;
    m_indexCapacityBytes = 0;
    m_indexCount = 0U;
}

bool LarZoneGpuLayer::compileProgram() {
    m_program = new QOpenGLShaderProgram(this);
    bool vertexCompiled =
        m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderLegacy);
    bool fragmentCompiled =
        m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderLegacy);
    m_program->bindAttributeLocation("aPos", 0);
    bool linked = m_program->link();

    if (!vertexCompiled || !fragmentCompiled || !linked) {
        delete m_program;
        m_program = new QOpenGLShaderProgram(this);
        vertexCompiled =
            m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderCore);
        fragmentCompiled =
            m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderCore);
        m_program->bindAttributeLocation("aPos", 0);
        linked = m_program->link();
    }
    if (!vertexCompiled || !fragmentCompiled || !linked) {
        emit diagnosticRaised(
            QStringLiteral("LAR overlay shader initialization failed: %1").arg(m_program->log()));
        return false;
    }

    m_positionAttribute = m_program->attributeLocation("aPos");
    m_sphereModeUniform = m_program->uniformLocation("uSphereMode");
    m_worldOffsetUniform = m_program->uniformLocation("uWorldOffsetX");
    m_sphereLatitudeSinCosUniform = m_program->uniformLocation("uSphereLatitudeSinCos");
    m_cameraBearingUniform = m_program->uniformLocation("uCameraBearing");
    m_positionOriginDeltaUniform = m_program->uniformLocation("uPositionOriginDelta");
    m_projectionUniform = m_program->uniformLocation("uProjection");
    m_colorUniform = m_program->uniformLocation("uColor");

    const bool complete = m_positionAttribute >= 0 && m_sphereModeUniform >= 0 &&
                          m_worldOffsetUniform >= 0 && m_sphereLatitudeSinCosUniform >= 0 &&
                          m_cameraBearingUniform >= 0 && m_positionOriginDeltaUniform >= 0 &&
                          m_projectionUniform >= 0 && m_colorUniform >= 0;

    if (!complete) {
        emit diagnosticRaised(QStringLiteral("LAR overlay shader interface is incomplete."));
    }
    return complete;
}

bool LarZoneGpuLayer::ensureBuffers() {
    if (!m_vertexBuffer.create()) {
        emit diagnosticRaised(QStringLiteral("Could not create the LAR overlay vertex buffer."));
        return false;
    }
    if (!m_indexBuffer.create()) {
        emit diagnosticRaised(QStringLiteral("Could not create the LAR overlay index buffer."));
        return false;
    }
    if (!m_vertexArray.create()) {
        emit diagnosticRaised(QStringLiteral("Could not create the LAR overlay vertex array."));
        return false;
    }
    m_vertexBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_indexBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    QOpenGLVertexArrayObject::Binder binder(&m_vertexArray);
    m_vertexBuffer.bind();
    m_program->enableAttributeArray(m_positionAttribute);
    m_program->setAttributeBuffer(m_positionAttribute, GL_FLOAT, 0, VertexStrideFloats,
                                  VertexStrideBytes);
    m_indexBuffer.bind();
    return true;
}

bool LarZoneGpuLayer::writeBuffer(QOpenGLBuffer &buffer, int &capacityBytes, const void *data,
                                  std::size_t sizeBytes, const QString &description) {
    if (sizeBytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        emit diagnosticRaised(
            QStringLiteral("%1 exceeds the OpenGL upload limit.").arg(description));
        return false;
    }
    const int requiredBytes = static_cast<int>(sizeBytes);
    if (!buffer.bind()) {
        emit diagnosticRaised(QStringLiteral("Could not bind %1.").arg(description));
        return false;
    }
    if (requiredBytes > capacityBytes) {
        capacityBytes = retainedCapacity(requiredBytes);
        buffer.allocate(capacityBytes);
    }
    if (requiredBytes > 0) {
        buffer.write(0, data, requiredBytes);
    }
    return true;
}

bool LarZoneGpuLayer::upload(const LarZoneMesh &mesh) {
    m_indexCount = 0U;
    if (mesh.empty()) {
        return true;
    }
    if (!meshIsValid(mesh)) {
        emit diagnosticRaised(QStringLiteral("The LAR overlay mesh exceeds renderer limits."));
        return false;
    }
    if (m_program == nullptr || !m_vertexArray.isCreated()) {
        emit diagnosticRaised(QStringLiteral("LAR overlay GPU resources are unavailable."));
        return false;
    }

    QOpenGLVertexArrayObject::Binder binder(&m_vertexArray);
    if (!writeBuffer(m_vertexBuffer, m_vertexCapacityBytes, mesh.vertices.data(),
                     mesh.vertices.size() * sizeof(float),
                     QStringLiteral("the LAR overlay vertex buffer")) ||
        !writeBuffer(m_indexBuffer, m_indexCapacityBytes, mesh.indices.data(),
                     mesh.indices.size() * sizeof(std::uint32_t),
                     QStringLiteral("the LAR overlay index buffer"))) {

        return false;
    }

    m_inRangeFill = mesh.inRangeFill;
    m_inZoneFill = mesh.inZoneFill;
    m_inRangeLines = mesh.inRangeLines;
    m_inZoneLines = mesh.inZoneLines;
    m_coordinateSpace = mesh.coordinateSpace;
    m_coordinateOrigin = mesh.coordinateOrigin;
    m_indexCount = mesh.indices.size();
    return true;
}

void LarZoneGpuLayer::drawRange(const LarZoneDrawRange &range, unsigned int primitive,
                                const QColor &color, const lar::map::WorldCopyRange &copies) {
    if (range.indexCount == 0U || range.firstIndex > m_indexCount ||
        range.indexCount > m_indexCount - range.firstIndex ||
        range.indexCount > static_cast<std::size_t>(std::numeric_limits<int>::max())) {

        return;
    }
    m_program->setUniformValue(m_colorUniform, colorVector(color));
    for (int copy = copies.first; copy <= copies.last; ++copy) {
        m_program->setUniformValue(m_worldOffsetUniform, static_cast<float>(copy * 360.0));
        glDrawElements(primitive, static_cast<int>(range.indexCount), GL_UNSIGNED_INT,
                       reinterpret_cast<const void *>(range.firstIndex * sizeof(std::uint32_t)));
    }
}

void LarZoneGpuLayer::draw(const LarZoneRenderState &state) {
    if (m_program == nullptr || !m_vertexArray.isCreated() || m_indexCount == 0U) {
        return;
    }

    const LarZoneCoordinateSpace expectedSpace =
        state.sphere ? LarZoneCoordinateSpace::SphereCameraRelative
                     : LarZoneCoordinateSpace::MercatorCameraRelative;
    if (m_coordinateSpace != expectedSpace)
        return;

    const QPointF cameraOrigin = state.sphere ? state.sphereCenterDegrees : state.flatCenter;
    const QPointF originDelta = m_coordinateOrigin - cameraOrigin;
    m_program->bind();
    m_program->setUniformValue(m_sphereModeUniform, state.sphere);
    m_program->setUniformValue(m_sphereLatitudeSinCosUniform,
                               state.sphereProjection.latitudeSinCos);
    m_program->setUniformValue(m_cameraBearingUniform, state.bearingRadians);
    m_program->setUniformValue(
        m_positionOriginDeltaUniform,
        QVector2D(static_cast<float>(originDelta.x()), static_cast<float>(originDelta.y())));
    m_program->setUniformValue(m_projectionUniform, state.projection);

    QOpenGLVertexArrayObject::Binder binder(&m_vertexArray);
    m_indexBuffer.bind();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    drawRange(m_inRangeFill, GL_TRIANGLES, QColor(52, 109, 145, 40), state.worldCopies);
    drawRange(m_inZoneFill, GL_TRIANGLES, QColor(20, 131, 102, 76), state.worldCopies);
    glLineWidth(2.0F);
    drawRange(m_inRangeLines, GL_LINES, QColor(QStringLiteral("#346d91")), state.worldCopies);
    drawRange(m_inZoneLines, GL_LINES, QColor(QStringLiteral("#0e755b")), state.worldCopies);

    m_program->setUniformValue(m_worldOffsetUniform, 0.0F);
    glDisable(GL_BLEND);
    m_program->release();
}
