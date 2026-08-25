
#include "viewer/viewport/lar_parametric_zone_gpu_layer.h"

#include "viewer/lar_geodesic_geometry.h"
#include "viewer/map/map_projection.h"
#include "viewer/viewport/geodesic_zone_sampler.h"
#include "viewer/viewport/lar_zone_input_validator.h"
#include "viewer/viewport/lar_zone_mesh_limits.h"

#include <QBitArray>
#include <QColor>
#include <QOpenGLShaderProgram>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr int RadialSegments = 24;
constexpr int AngularSegments = 128;
constexpr double MercatorLimitRadians = 1.4844222297453323; // 85.05112878°
constexpr double EarthRadius = LarGeodesicGeometry::EarthRadiusMeters;
constexpr double Pi = LarGeodesicGeometry::Pi;
constexpr double TwoPi = LarGeodesicGeometry::TwoPi;

const char *vertexShaderLegacy = R"(
    attribute vec2 aParam;
    uniform bool uSphereMode;
    uniform float uWorldOffsetX;
    uniform vec2 uSphereCenterHigh;
    uniform vec2 uSphereCenterLow;
    uniform vec2 uSphereLatitudeSinCos;
    uniform float uCameraBearing;
    uniform vec2 uFlatCenter;
    uniform mat4 uProjection;
    uniform vec2 uZoneCenter;
    uniform vec2 uZoneRadii;
    uniform vec2 uStartSpan;
    varying float vZ;

    vec2 rotateForCamera(vec2 value) {
        float cosine = cos(uCameraBearing);
        float sine = sin(uCameraBearing);
        return vec2(value.x * cosine - value.y * sine,
                    value.x * sine + value.y * cosine);
    }

    vec3 projectSphere(vec2 coordinateDegrees) {
        float degreesToRadians = 3.14159265358979323846 / 180.0;
        vec2 delta = ((coordinateDegrees - uSphereCenterHigh) - uSphereCenterLow)
                     * degreesToRadians;
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
        const float pi = 3.14159265358979323846;
        const float earthRadius = 6371000.0;
        float distance = mix(uZoneRadii.x, uZoneRadii.y, aParam.x) / earthRadius;
        float bearing = uStartSpan.x + uStartSpan.y * aParam.y;
        float sinLat = sin(uZoneCenter.x);
        float cosLat = cos(uZoneCenter.x);
        float sinDistance = sin(distance);
        float cosDistance = cos(distance);
        float destinationLat = asin(clamp(sinLat * cosDistance
                                          + cosLat * sinDistance * cos(bearing), -1.0, 1.0));
        float lonOffset = atan(sin(bearing) * sinDistance * cosLat,
                               cosDistance - sinLat * sin(destinationLat));
        float longitude = uZoneCenter.y + lonOffset;
        if (!uSphereMode) {
            float latitude = clamp(destinationLat, -1.4844222297453323,
                                   1.4844222297453323);
            float mercatorY = degrees(log(tan(pi * 0.25 + latitude * 0.5)));
            float longitudeDegrees = degrees(longitude) + uWorldOffsetX;
            vec2 mapPosition = vec2(longitudeDegrees, mercatorY);
            vec2 viewPosition = rotateForCamera(mapPosition - uFlatCenter);
            vZ = 1.0;
            gl_Position = uProjection * vec4(viewPosition, 0.0, 1.0);
        } else {
            vec3 spherePosition = projectSphere(vec2(degrees(longitude), degrees(destinationLat)));
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
        if (vZ < 0.0) discard;
        gl_FragColor = uColor;
    }
    )";

const char *vertexShaderCore = R"(#version 150
    in vec2 aParam;
    uniform bool uSphereMode;
    uniform float uWorldOffsetX;
    uniform vec2 uSphereCenterHigh;
    uniform vec2 uSphereCenterLow;
    uniform vec2 uSphereLatitudeSinCos;
    uniform float uCameraBearing;
    uniform vec2 uFlatCenter;
    uniform mat4 uProjection;
    uniform vec2 uZoneCenter;
    uniform vec2 uZoneRadii;
    uniform vec2 uStartSpan;
    out float vZ;
    vec2 rotateForCamera(vec2 value) {
        float cosine = cos(uCameraBearing);
        float sine = sin(uCameraBearing);
        return vec2(value.x * cosine - value.y * sine,
                    value.x * sine + value.y * cosine);
    }
    vec3 projectSphere(vec2 coordinateDegrees) {
        float degreesToRadians = 3.14159265358979323846 / 180.0;
        vec2 delta = ((coordinateDegrees - uSphereCenterHigh) - uSphereCenterLow)
                     * degreesToRadians;
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
        const float pi = 3.14159265358979323846;
        const float earthRadius = 6371000.0;
        float distance = mix(uZoneRadii.x, uZoneRadii.y, aParam.x) / earthRadius;
        float bearing = uStartSpan.x + uStartSpan.y * aParam.y;
        float sinLat = sin(uZoneCenter.x);
        float cosLat = cos(uZoneCenter.x);
        float sinDistance = sin(distance);
        float cosDistance = cos(distance);
        float destinationLat = asin(clamp(sinLat * cosDistance
                                          + cosLat * sinDistance * cos(bearing), -1.0, 1.0));
        float lonOffset = atan(sin(bearing) * sinDistance * cosLat,
                               cosDistance - sinLat * sin(destinationLat));
        float longitude = uZoneCenter.y + lonOffset;
        if (!uSphereMode) {
            float latitude = clamp(destinationLat, -1.4844222297453323,
                                   1.4844222297453323);
            float mercatorY = degrees(log(tan(pi * 0.25 + latitude * 0.5)));
            vec2 mapPosition = vec2(degrees(longitude) + uWorldOffsetX, mercatorY);
            vec2 viewPosition = rotateForCamera(mapPosition - uFlatCenter);
            vZ = 1.0;
            gl_Position = uProjection * vec4(viewPosition, 0.0, 1.0);
        } else {
            vec3 spherePosition = projectSphere(vec2(degrees(longitude), degrees(destinationLat)));
            vZ = spherePosition.z;
            gl_Position = uProjection
                          * vec4(rotateForCamera(spherePosition.xy), 0.0, 1.0);
        }
    }
    )";

const char *fragmentShaderCore = R"(#version 150
    uniform vec4 uColor;
    in float vZ;
    out vec4 fragColor;
    void main() {
        if (vZ < 0.0) discard;
        fragColor = uColor;
    }
    )";

bool projectPoint(const lar::map::MapCamera &camera, const GeoCoordinateRadians &point, int width,
                  int height, QPointF *screen) {
    bool visible = false;
    return screen != nullptr && camera.projectGeoToScreen(qRadiansToDegrees(point.longitude),
                                                          qRadiansToDegrees(point.latitude), width,
                                                          height, *screen, visible);
}

double flatFloatCoordinateError(const lar::map::MapCamera &camera,
                                const GeoCoordinateRadians &point, int width, int height) {
    const auto viewport = camera.mercatorViewport(width, height);
    const QPointF center = camera.mercatorCenter();
    const double longitudeDegrees = qRadiansToDegrees(point.longitude);
    const double unwrapped = lar::map::MapProjection::unwrapLongitude(longitudeDegrees, center.x());
    const double worldOffset =
        std::round((unwrapped - longitudeDegrees) / lar::map::MapProjection::WorldWidthDegrees) *
        lar::map::MapProjection::WorldWidthDegrees;
    const double mercatorY =
        lar::map::MapProjection::projectLatitude(qRadiansToDegrees(point.latitude));
    const double exactX = unwrapped - center.x();
    const double exactY = mercatorY - center.y();
    const double floatX =
        static_cast<double>(static_cast<float>(longitudeDegrees) + static_cast<float>(worldOffset) -
                            static_cast<float>(center.x()));
    const double floatY =
        static_cast<double>(static_cast<float>(mercatorY) - static_cast<float>(center.y()));
    const double bearing = qDegreesToRadians(static_cast<double>(camera.bearingDegrees()));
    const double cosine = std::cos(bearing);
    const double sine = std::sin(bearing);
    const double errorX = floatX - exactX;
    const double errorY = floatY - exactY;
    const double viewErrorX = errorX * cosine - errorY * sine;
    const double viewErrorY = errorX * sine + errorY * cosine;
    return std::hypot(viewErrorX * static_cast<double>(width) / (2.0 * viewport.halfWidth),
                      viewErrorY * static_cast<double>(height) / (2.0 * viewport.halfHeight));
}

bool fixedTopologyMeetsAccuracy(const LarZoneDefinition &zone, const lar::map::MapCamera &camera,
                                int width, int height) {
    if (width <= 0 || height <= 0)
        return false;
    const GeodesicZoneSampleGrid adaptive =
        GeodesicZoneSampler().sample(zone, camera, width, height);
    if (adaptive.rowCount() < 2U || adaptive.columnCount() < 2U ||
        adaptive.rowCount() - 1U > static_cast<std::size_t>(RadialSegments / 2) ||
        adaptive.columnCount() - 1U > static_cast<std::size_t>(AngularSegments / 2)) {
        return false;
    }

    constexpr double RoundingBudgetPixels = LarZoneMeshLimits::CurveErrorPixels * 0.35;
    LarZoneDefinition rounded = zone;
    rounded.center.latitude = static_cast<double>(static_cast<float>(zone.center.latitude));
    rounded.center.longitude = static_cast<double>(static_cast<float>(zone.center.longitude));
    rounded.innerRadiusMeters = static_cast<double>(static_cast<float>(zone.innerRadiusMeters));
    rounded.outerRadiusMeters = static_cast<double>(static_cast<float>(zone.outerRadiusMeters));
    rounded.startBearingRadians = static_cast<double>(static_cast<float>(zone.startBearingRadians));
    rounded.spanRadians = static_cast<double>(static_cast<float>(zone.spanRadians));

    for (const double radius : {zone.innerRadiusMeters, zone.outerRadiusMeters}) {
        const double roundedRadius = radius == zone.innerRadiusMeters ? rounded.innerRadiusMeters
                                                                      : rounded.outerRadiusMeters;
        for (int sample = 0; sample <= 16; ++sample) {
            const double fraction = static_cast<double>(sample) / 16.0;
            const GeoCoordinateRadians precisePoint = LarGeodesicGeometry::destination(
                zone.center, radius, zone.startBearingRadians + zone.spanRadians * fraction);
            const GeoCoordinateRadians roundedPoint = LarGeodesicGeometry::destination(
                rounded.center, roundedRadius,
                rounded.startBearingRadians + rounded.spanRadians * fraction);
            QPointF preciseScreen;
            QPointF roundedScreen;
            if (!projectPoint(camera, precisePoint, width, height, &preciseScreen) ||
                !projectPoint(camera, roundedPoint, width, height, &roundedScreen) ||
                std::hypot(preciseScreen.x() - roundedScreen.x(),
                           preciseScreen.y() - roundedScreen.y()) > RoundingBudgetPixels) {
                return false;
            }
            if (camera.presentation() == lar::map::MapPresentation::Mercator &&
                flatFloatCoordinateError(camera, precisePoint, width, height) >
                    RoundingBudgetPixels) {
                return false;
            }
        }
    }
    return true;
}

QVector4D colorVector(const QColor &color) {
    return {float(color.redF()), float(color.greenF()), float(color.blueF()),
            float(color.alphaF())};
}

void appendGrid(std::vector<float> &vertices, std::vector<std::uint32_t> &indices,
                LarZoneDrawRange &fill, LarZoneDrawRange &lines, bool annular, bool fullCircle,
                LarZoneDrawRange *fullCircleLines = nullptr) {
    const std::uint32_t firstVertex = static_cast<std::uint32_t>(vertices.size() / 2U);
    for (int row = 0; row <= RadialSegments; ++row) {
        for (int column = 0; column <= AngularSegments; ++column) {
            vertices.push_back(float(row) / float(RadialSegments));
            vertices.push_back(float(column) / float(AngularSegments));
        }
    }
    fill.firstIndex = indices.size();
    for (int row = 0; row < RadialSegments; ++row) {
        for (int column = 0; column < AngularSegments; ++column) {
            const std::uint32_t a =
                firstVertex + std::uint32_t(row * (AngularSegments + 1) + column);
            const std::uint32_t b = a + 1U;
            const std::uint32_t c = a + std::uint32_t(AngularSegments + 1);
            const std::uint32_t d = c + 1U;
            indices.insert(indices.end(), {a, c, b, c, d, b});
        }
    }
    fill.indexCount = indices.size() - fill.firstIndex;

    lines.firstIndex = indices.size();
    const std::uint32_t outer = firstVertex + std::uint32_t(RadialSegments * (AngularSegments + 1));
    for (int column = 0; column < AngularSegments; ++column)
        indices.insert(indices.end(),
                       {outer + std::uint32_t(column), outer + std::uint32_t(column + 1)});
    if (annular) {
        const std::uint32_t inner = firstVertex;
        for (int column = 0; column < AngularSegments; ++column) {
            indices.insert(indices.end(),
                           {inner + std::uint32_t(column), inner + std::uint32_t(column + 1)});
        }
    }
    if (fullCircleLines != nullptr) {
        fullCircleLines->firstIndex = lines.firstIndex;
        fullCircleLines->indexCount = indices.size() - fullCircleLines->firstIndex;
    }
    if (!fullCircle) {
        for (int column : {0, AngularSegments}) {
            for (int row = 0; row < RadialSegments; ++row) {
                const std::uint32_t a =
                    firstVertex + std::uint32_t(row * (AngularSegments + 1) + column);
                const std::uint32_t b = a + std::uint32_t(AngularSegments + 1);
                indices.insert(indices.end(), {a, b});
            }
        }
    }
    lines.indexCount = indices.size() - lines.firstIndex;
}
} // namespace

LarParametricZoneGpuLayer::LarParametricZoneGpuLayer(QObject *parent) : QObject(parent) {}

LarParametricZoneGpuLayer::~LarParametricZoneGpuLayer() {
    delete m_program;
}

bool LarParametricZoneGpuLayer::setZones(const Target &target, const QBitArray &availableFields,
                                         const lar::map::MapCamera &camera, int viewportWidth,
                                         int viewportHeight) {
    m_inRange = {};
    m_inZone = {};
    m_hasZones = false;
    const LarZoneValidationResult validated =
        LarZoneInputValidator().validate(target, availableFields);
    m_eligible = !validated.inputRejected;
    m_minimumLongitude = 0.0;
    m_maximumLongitude = 0.0;

    const auto regular = [&camera, viewportWidth, viewportHeight](
                             const LarZoneDefinition &zone, LarParametricZoneState *destination) {
        const double radiusAngle = zone.outerRadiusMeters / EarthRadius;
        if (std::abs(zone.center.latitude) + radiusAngle >= Pi * 0.5 - qDegreesToRadians(0.05) ||
            (camera.presentation() == lar::map::MapPresentation::Mercator &&
             std::abs(zone.center.latitude) + radiusAngle >=
                 MercatorLimitRadians - qDegreesToRadians(0.05)) ||
            !fixedTopologyMeetsAccuracy(zone, camera, viewportWidth, viewportHeight)) {
            return false;
        }
        destination->valid = true;
        destination->centerRadians = {float(zone.center.latitude), float(zone.center.longitude)};
        destination->radiiMeters = {float(zone.innerRadiusMeters), float(zone.outerRadiusMeters)};
        destination->startAndSpanRadians = {float(zone.startBearingRadians),
                                            float(zone.spanRadians)};
        destination->fullCircle = zone.spanRadians >= TwoPi - 1.0e-9;
        return true;
    };

    if (validated.inRange) {
        m_hasZones = true;
        m_eligible = regular(*validated.inRange, &m_inRange) && m_eligible;
    }
    if (validated.inZone) {
        m_hasZones = true;
        m_eligible = regular(*validated.inZone, &m_inZone) && m_eligible;
    }
    if (m_hasZones) {
        const auto include = [this](const LarParametricZoneState &zone) {
            if (!zone.valid)
                return;
            const double center = qRadiansToDegrees(double(zone.centerRadians.y()));
            const double radiusAngle = double(zone.radiiMeters.y()) / EarthRadius;
            // A longitude extent based only on radiusAngle is too narrow near
            // the poles.  Use the exact small-circle extremum when the circle
            // does not reach a pole, and conservatively cover the full world
            // once it does.  This keeps periodic copies gap-free for the
            // parametric path just as they are for the CPU mesh path.
            const double centerLatitude = std::abs(double(zone.centerRadians.x()));
            const double longitudeDelta =
                centerLatitude + radiusAngle >= Pi * 0.5
                    ? Pi
                    : std::asin(std::clamp(std::sin(radiusAngle) /
                                               std::max(1.0e-9, std::cos(centerLatitude)),
                                           -1.0, 1.0));
            const double radius = qRadiansToDegrees(longitudeDelta);
            if (m_minimumLongitude == 0.0 && m_maximumLongitude == 0.0) {
                m_minimumLongitude = center - radius;
                m_maximumLongitude = center + radius;
            } else {
                m_minimumLongitude = std::min(m_minimumLongitude, center - radius);
                m_maximumLongitude = std::max(m_maximumLongitude, center + radius);
            }
        };
        include(m_inRange);
        include(m_inZone);
    }
    return m_eligible;
}

bool LarParametricZoneGpuLayer::longitudeBounds(double *minimum, double *maximum) const noexcept {
    if (!m_hasZones || minimum == nullptr || maximum == nullptr)
        return false;
    *minimum = m_minimumLongitude;
    *maximum = m_maximumLongitude;
    return true;
}

bool LarParametricZoneGpuLayer::initialize() {
    initializeOpenGLFunctions();
    cleanup();
    if (!compileProgram() || !buildTopology())
        return false;
    m_initialized = true;
    return true;
}

void LarParametricZoneGpuLayer::cleanup() {
    m_initialized = false;
    m_vertexArray.destroy();
    m_vertexBuffer.destroy();
    m_indexBuffer.destroy();
    delete m_program;
    m_program = nullptr;
    m_indexCount = 0;
}

bool LarParametricZoneGpuLayer::compileProgram() {
    m_program = new QOpenGLShaderProgram(this);
    bool vertex = m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderLegacy);
    bool fragment =
        m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderLegacy);
    m_program->bindAttributeLocation("aParam", 0);
    bool linked = m_program->link();
    if (!vertex || !fragment || !linked) {
        delete m_program;
        m_program = new QOpenGLShaderProgram(this);
        vertex = m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderCore);
        fragment = m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderCore);
        m_program->bindAttributeLocation("aParam", 0);
        linked = m_program->link();
    }
    if (!vertex || !fragment || !linked) {
        emit diagnosticRaised(QStringLiteral("Parametric LAR shader initialization failed: %1")
                                  .arg(m_program->log()));
        return false;
    }
    m_positionAttribute = m_program->attributeLocation("aParam");
    m_sphereModeUniform = m_program->uniformLocation("uSphereMode");
    m_worldOffsetUniform = m_program->uniformLocation("uWorldOffsetX");
    m_sphereCenterHighUniform = m_program->uniformLocation("uSphereCenterHigh");
    m_sphereCenterLowUniform = m_program->uniformLocation("uSphereCenterLow");
    m_sphereLatitudeSinCosUniform = m_program->uniformLocation("uSphereLatitudeSinCos");
    m_cameraBearingUniform = m_program->uniformLocation("uCameraBearing");
    m_flatCenterUniform = m_program->uniformLocation("uFlatCenter");
    m_projectionUniform = m_program->uniformLocation("uProjection");
    m_zoneCenterUniform = m_program->uniformLocation("uZoneCenter");
    m_zoneRadiiUniform = m_program->uniformLocation("uZoneRadii");
    m_startSpanUniform = m_program->uniformLocation("uStartSpan");
    m_colorUniform = m_program->uniformLocation("uColor");
    return m_positionAttribute >= 0 && m_sphereModeUniform >= 0 && m_worldOffsetUniform >= 0 &&
           m_sphereCenterHighUniform >= 0 && m_sphereCenterLowUniform >= 0 &&
           m_sphereLatitudeSinCosUniform >= 0 && m_cameraBearingUniform >= 0 &&
           m_flatCenterUniform >= 0 && m_projectionUniform >= 0 && m_zoneCenterUniform >= 0 &&
           m_zoneRadiiUniform >= 0 && m_startSpanUniform >= 0 && m_colorUniform >= 0;
}

bool LarParametricZoneGpuLayer::buildTopology() {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(2U * (RadialSegments + 1) * (AngularSegments + 1) * 2U);
    indices.reserve(2U * RadialSegments * AngularSegments * 6U);
    appendGrid(vertices, indices, m_inRangeFill, m_inRangeLines, false, true);
    appendGrid(vertices, indices, m_inZoneFill, m_inZoneLines, true, false, &m_inZoneFullLines);
    if (!m_vertexBuffer.create() || !m_indexBuffer.create() || !m_vertexArray.create()) {
        emit diagnosticRaised(QStringLiteral("Could not create parametric LAR buffers."));
        return false;
    }
    QOpenGLVertexArrayObject::Binder binder(&m_vertexArray);
    m_vertexBuffer.bind();
    m_vertexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vertexBuffer.allocate(vertices.data(), int(vertices.size() * sizeof(float)));
    m_indexBuffer.bind();
    m_indexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_indexBuffer.allocate(indices.data(), int(indices.size() * sizeof(std::uint32_t)));
    m_program->enableAttributeArray(m_positionAttribute);
    m_program->setAttributeBuffer(m_positionAttribute, GL_FLOAT, 0, 2, 2 * int(sizeof(float)));
    m_indexCount = indices.size();
    return true;
}

void LarParametricZoneGpuLayer::drawRange(const LarParametricZoneState &zone,
                                          const LarZoneDrawRange &range, unsigned int primitive,
                                          const QColor &color,
                                          const lar::map::WorldCopyRange &copies) {
    if (!zone.valid || range.indexCount == 0U)
        return;
    m_program->setUniformValue(m_zoneCenterUniform, zone.centerRadians);
    m_program->setUniformValue(m_zoneRadiiUniform, zone.radiiMeters);
    m_program->setUniformValue(m_startSpanUniform, zone.startAndSpanRadians);
    m_program->setUniformValue(m_colorUniform, colorVector(color));
    for (int copy = copies.first; copy <= copies.last; ++copy) {
        m_program->setUniformValue(m_worldOffsetUniform, float(copy * 360.0));
        glDrawElements(primitive, int(range.indexCount), GL_UNSIGNED_INT,
                       reinterpret_cast<const void *>(range.firstIndex * sizeof(std::uint32_t)));
    }
}

void LarParametricZoneGpuLayer::draw(const LarZoneRenderState &state) {
    if (!m_initialized || !m_eligible || !m_hasZones || m_program == nullptr)
        return;
    m_program->bind();
    m_program->setUniformValue(m_sphereModeUniform, state.sphere);
    m_program->setUniformValue(m_sphereCenterHighUniform, state.sphereProjection.centerHighDegrees);
    m_program->setUniformValue(m_sphereCenterLowUniform, state.sphereProjection.centerLowDegrees);
    m_program->setUniformValue(m_sphereLatitudeSinCosUniform,
                               state.sphereProjection.latitudeSinCos);
    m_program->setUniformValue(m_cameraBearingUniform, state.bearingRadians);
    m_program->setUniformValue(m_flatCenterUniform,
                               QVector2D(float(state.flatCenter.x()), float(state.flatCenter.y())));
    m_program->setUniformValue(m_projectionUniform, state.projection);
    QOpenGLVertexArrayObject::Binder binder(&m_vertexArray);
    m_indexBuffer.bind();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawRange(m_inRange, m_inRangeFill, GL_TRIANGLES, QColor(52, 109, 145, 40), state.worldCopies);
    drawRange(m_inZone, m_inZoneFill, GL_TRIANGLES, QColor(20, 131, 102, 76), state.worldCopies);
    glLineWidth(2.0F);
    drawRange(m_inRange, m_inRangeLines, GL_LINES, QColor(QStringLiteral("#346d91")),
              state.worldCopies);
    const LarZoneDrawRange &inZoneLines = m_inZone.fullCircle ? m_inZoneFullLines : m_inZoneLines;
    drawRange(m_inZone, inZoneLines, GL_LINES, QColor(QStringLiteral("#0e755b")),
              state.worldCopies);
    glDisable(GL_BLEND);
    m_program->release();
}
