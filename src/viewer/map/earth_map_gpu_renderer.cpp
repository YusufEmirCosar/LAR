
#include "viewer/map/earth_map_gpu_renderer.h"

#include "viewer/map/map_asset_limits.h"
#include "viewer/map/map_palette.h"
#include "viewer/map/map_projection.h"
#include "viewer/map/map_shaders.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace lar::map {
namespace {

constexpr int VertexStrideFloats = 3;
constexpr int VertexStrideBytes = VertexStrideFloats * static_cast<int>(sizeof(float));

void setError(QString *destination, const QString &message) {
    if (destination != nullptr) {
        *destination = message;
    }
}

bool fitsOpenGlBuffer(std::size_t elementCount, std::size_t elementSize) noexcept {
    return elementCount <= static_cast<std::size_t>(std::numeric_limits<int>::max()) / elementSize;
}

bool indicesAreValid(const std::vector<std::uint32_t> &indices, std::size_t vertexCount) noexcept {
    for (const std::uint32_t index : indices) {
        if (static_cast<std::size_t>(index) >= vertexCount) {
            return false;
        }
    }
    return true;
}

bool verticesAreValid(const std::vector<float> &vertices) noexcept {
    for (std::size_t offset = 0U; offset + 2U < vertices.size(); offset += 3U) {
        if (!std::isfinite(vertices[offset]) || !std::isfinite(vertices[offset + 1U]) ||
            !std::isfinite(vertices[offset + 2U]) ||
            std::abs(vertices[offset]) > limits::MaximumAbsoluteLongitude ||
            std::abs(vertices[offset + 1U]) > limits::MaximumAbsoluteLatitude ||
            std::abs(vertices[offset + 2U]) > limits::MaximumAbsoluteMercatorY) {

            return false;
        }
    }
    return true;
}

} // namespace

EarthMapGpuRenderer::EarthMapGpuRenderer() = default;

EarthMapGpuRenderer::~EarthMapGpuRenderer() {
    m_program.reset();
}

bool EarthMapGpuRenderer::setMesh(std::shared_ptr<const MapMesh> mesh, QString *errorMessage) {
    if (mesh == nullptr || mesh->empty() || mesh->vertices.size() % 3U != 0U ||
        mesh->vertexCount() > limits::MaximumVertexCount ||
        mesh->mercatorFillIndices.size() > limits::MaximumMercatorIndexCount ||
        mesh->sphereFillIndices.size() > limits::MaximumSphereIndexCount ||
        mesh->borderIndices.size() > limits::MaximumBorderIndexCount ||
        mesh->mercatorFillIndices.size() % 3U != 0U || mesh->sphereFillIndices.size() % 3U != 0U ||
        mesh->borderIndices.size() % 2U != 0U ||
        !fitsOpenGlBuffer(mesh->vertices.size(), sizeof(float)) ||
        !fitsOpenGlBuffer(mesh->mercatorFillIndices.size(), sizeof(std::uint32_t)) ||
        !fitsOpenGlBuffer(mesh->sphereFillIndices.size(), sizeof(std::uint32_t)) ||
        !fitsOpenGlBuffer(mesh->borderIndices.size(), sizeof(std::uint32_t)) ||
        !verticesAreValid(mesh->vertices) ||
        mesh->mercatorFillIndices.size() >
            static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
        mesh->sphereFillIndices.size() >
            static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
        mesh->borderIndices.size() >
            static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
        !indicesAreValid(mesh->mercatorFillIndices, mesh->vertexCount()) ||
        !indicesAreValid(mesh->sphereFillIndices, mesh->vertexCount()) ||
        !indicesAreValid(mesh->borderIndices, mesh->vertexCount())) {

        setError(errorMessage, QStringLiteral("The map mesh exceeds renderer limits."));
        return false;
    }

    m_mesh = std::move(mesh);
    m_pendingUpload = true;
    return true;
}

bool EarthMapGpuRenderer::compileProgram(QString *errorMessage) {
    m_program = std::make_unique<QOpenGLShaderProgram>();
    bool vertexCompiled =
        m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, shaders::VertexLegacy);
    bool fragmentCompiled =
        m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, shaders::FragmentLegacy);
    m_program->bindAttributeLocation("aPosition", 0);
    bool linked = m_program->link();

    if (!vertexCompiled || !fragmentCompiled || !linked) {
        m_program = std::make_unique<QOpenGLShaderProgram>();
        vertexCompiled =
            m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, shaders::VertexCore);
        fragmentCompiled =
            m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, shaders::FragmentCore);
        m_program->bindAttributeLocation("aPosition", 0);
        linked = m_program->link();
    }

    if (!vertexCompiled || !fragmentCompiled || !linked) {
        setError(errorMessage,
                 QStringLiteral("Map shader initialization failed: %1").arg(m_program->log()));
        m_program.reset();
        return false;
    }
    return true;
}

bool EarthMapGpuRenderer::cacheProgramInputs(QString *errorMessage) {
    m_positionAttribute = m_program->attributeLocation("aPosition");
    m_sphereUniform = m_program->uniformLocation("uSphere");
    m_screenSpaceUniform = m_program->uniformLocation("uScreenSpace");
    m_worldOffsetUniform = m_program->uniformLocation("uWorldOffset");
    m_sphereCenterHighUniform = m_program->uniformLocation("uSphereCenterHigh");
    m_sphereCenterLowUniform = m_program->uniformLocation("uSphereCenterLow");
    m_sphereLatitudeSinCosUniform = m_program->uniformLocation("uSphereLatitudeSinCos");
    m_bearingUniform = m_program->uniformLocation("uBearing");
    m_mercatorCenterUniform = m_program->uniformLocation("uMercatorCenter");
    m_projectionUniform = m_program->uniformLocation("uProjection");
    m_colorUniform = m_program->uniformLocation("uColor");
    m_keepBackUniform = m_program->uniformLocation("uKeepBack");

    if (m_positionAttribute < 0 || m_sphereUniform < 0 || m_screenSpaceUniform < 0 ||
        m_worldOffsetUniform < 0 || m_sphereCenterHighUniform < 0 || m_sphereCenterLowUniform < 0 ||
        m_sphereLatitudeSinCosUniform < 0 || m_bearingUniform < 0 || m_mercatorCenterUniform < 0 ||
        m_projectionUniform < 0 || m_colorUniform < 0 || m_keepBackUniform < 0) {

        setError(errorMessage, QStringLiteral("The map shader is missing a required input."));
        return false;
    }
    return true;
}

bool EarthMapGpuRenderer::initialize(QString *errorMessage) {
    initializeOpenGLFunctions();
    cleanup();
    if (!compileProgram(errorMessage) || !cacheProgramInputs(errorMessage)) {
        m_shaderValid = false;
        return false;
    }

    m_shaderValid = true;
    return uploadPending(errorMessage);
}

bool EarthMapGpuRenderer::uploadIndexBuffer(QOpenGLBuffer &buffer,
                                            const std::vector<std::uint32_t> &indices,
                                            const QString &label, QString *errorMessage) {
    if (indices.empty()) {
        return true;
    }
    if (!buffer.isCreated() && !buffer.create()) {
        setError(errorMessage, QStringLiteral("Failed to create the %1 index buffer.").arg(label));
        return false;
    }
    if (!buffer.bind()) {
        setError(errorMessage, QStringLiteral("Failed to bind the %1 index buffer.").arg(label));
        return false;
    }
    buffer.allocate(indices.data(), static_cast<int>(indices.size() * sizeof(std::uint32_t)));
    return true;
}

bool EarthMapGpuRenderer::createOceanDisc(QString *errorMessage) {
    std::vector<float> circle;
    constexpr int SegmentCount = 96;
    circle.reserve(static_cast<std::size_t>(SegmentCount + 2) * 3U);
    circle.insert(circle.end(), {0.0F, 0.0F, 0.0F});
    for (int index = 0; index <= SegmentCount; ++index) {
        const float angle = static_cast<float>(index) * 2.0F * static_cast<float>(M_PI) /
                            static_cast<float>(SegmentCount);
        circle.push_back(std::cos(angle));
        circle.push_back(std::sin(angle));
        circle.push_back(0.0F);
    }
    m_oceanVertexCount = circle.size() / 3U;

    if ((!m_oceanVertexBuffer.isCreated() && !m_oceanVertexBuffer.create()) ||
        (!m_oceanVertexArray.isCreated() && !m_oceanVertexArray.create())) {

        setError(errorMessage, QStringLiteral("Failed to create the ocean GPU geometry."));
        return false;
    }

    QOpenGLVertexArrayObject::Binder binder(&m_oceanVertexArray);
    m_oceanVertexBuffer.bind();
    m_oceanVertexBuffer.allocate(circle.data(), static_cast<int>(circle.size() * sizeof(float)));
    m_program->enableAttributeArray(m_positionAttribute);

    m_program->setAttributeBuffer(m_positionAttribute, GL_FLOAT, 0, VertexStrideFloats,
                                  VertexStrideBytes);

    return true;
}

bool EarthMapGpuRenderer::uploadPending(QString *errorMessage) {
    if (!m_pendingUpload) {
        return true;
    }
    if (!m_shaderValid || m_program == nullptr || m_mesh == nullptr) {
        setError(errorMessage, QStringLiteral("Map GPU resources are not ready for upload."));
        return false;
    }
    if ((!m_vertexBuffer.isCreated() && !m_vertexBuffer.create()) ||
        (!m_mapVertexArray.isCreated() && !m_mapVertexArray.create())) {

        setError(errorMessage, QStringLiteral("Failed to create map GPU geometry."));
        return false;
    }

    m_program->bind();
    {
        QOpenGLVertexArrayObject::Binder binder(&m_mapVertexArray);
        m_vertexBuffer.bind();
        m_vertexBuffer.allocate(m_mesh->vertices.data(),
                                static_cast<int>(m_mesh->vertices.size() * sizeof(float)));
        m_program->enableAttributeArray(m_positionAttribute);

        m_program->setAttributeBuffer(m_positionAttribute, GL_FLOAT, 0, VertexStrideFloats,
                                      VertexStrideBytes);
    }

    if (!uploadIndexBuffer(m_mercatorIndexBuffer, m_mesh->mercatorFillIndices,
                           QStringLiteral("Mercator fill"), errorMessage) ||
        !uploadIndexBuffer(m_sphereIndexBuffer, m_mesh->sphereFillIndices,
                           QStringLiteral("sphere fill"), errorMessage) ||
        !uploadIndexBuffer(m_borderIndexBuffer, m_mesh->borderIndices, QStringLiteral("border"),
                           errorMessage) ||
        !createOceanDisc(errorMessage)) {

        m_program->release();
        return false;
    }

    m_mercatorIndexCount = m_mesh->mercatorFillIndices.size();
    m_sphereIndexCount = m_mesh->sphereFillIndices.size();
    m_borderIndexCount = m_mesh->borderIndices.size();
    m_pendingUpload = false;
    m_program->release();
    return true;
}

QOpenGLBuffer &EarthMapGpuRenderer::fillIndexBuffer(MapPresentation presentation) {
    return presentation == MapPresentation::Sphere ? m_sphereIndexBuffer : m_mercatorIndexBuffer;
}

std::size_t EarthMapGpuRenderer::fillIndexCount(MapPresentation presentation) const noexcept {
    return presentation == MapPresentation::Sphere ? m_sphereIndexCount : m_mercatorIndexCount;
}

void EarthMapGpuRenderer::draw(const MapCamera &camera, int width, int height) {
    const bool sphere = camera.presentation() == MapPresentation::Sphere;
    const QVector4D &clearColor = sphere ? palette::Space : palette::Ocean;

    glClearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());

    glClear(GL_COLOR_BUFFER_BIT);

    const std::size_t fillCount = fillIndexCount(camera.presentation());

    if (!m_shaderValid || m_program == nullptr || fillCount == 0U || m_pendingUpload) {

        return;
    }

    m_program->bind();
    m_program->setUniformValue(m_screenSpaceUniform, false);
    m_program->setUniformValue(m_sphereUniform, sphere);
    m_program->setUniformValue(m_worldOffsetUniform, 0.0F);
    const SphereProjectionParameters sphereProjection = camera.sphereProjectionParameters();
    m_program->setUniformValue(m_sphereCenterHighUniform, sphereProjection.centerHighDegrees);
    m_program->setUniformValue(m_sphereCenterLowUniform, sphereProjection.centerLowDegrees);
    m_program->setUniformValue(m_sphereLatitudeSinCosUniform, sphereProjection.latitudeSinCos);
    m_program->setUniformValue(m_bearingUniform, qDegreesToRadians(camera.bearingDegrees()));
    m_program->setUniformValue(m_mercatorCenterUniform,
                               QVector2D(static_cast<float>(camera.mercatorCenter().x()),
                                         static_cast<float>(camera.mercatorCenter().y())));
    m_program->setUniformValue(m_projectionUniform, camera.projectionMatrix(width, height));

    if (sphere && m_oceanVertexBuffer.isCreated() && m_oceanVertexArray.isCreated()) {

        m_program->setUniformValue(m_keepBackUniform, true);
        m_program->setUniformValue(m_screenSpaceUniform, true);
        m_program->setUniformValue(m_colorUniform, palette::Ocean);
        QOpenGLVertexArrayObject::Binder oceanBinder(&m_oceanVertexArray);
        glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(m_oceanVertexCount));
        m_program->setUniformValue(m_screenSpaceUniform, false);
        m_program->setUniformValue(m_keepBackUniform, false);
    }

    QOpenGLVertexArrayObject::Binder mapBinder(&m_mapVertexArray);
    fillIndexBuffer(camera.presentation()).bind();
    m_program->setUniformValue(m_colorUniform, palette::Land);
    m_program->setUniformValue(m_keepBackUniform, false);
    const WorldCopyRange copies =
        sphere ? WorldCopyRange{0, 0} : camera.visibleWorldCopies(width, height);
    for (int copy = copies.first; copy <= copies.last; ++copy) {
        m_program->setUniformValue(m_worldOffsetUniform,
                                   static_cast<float>(copy * MapProjection::WorldWidthDegrees));

        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(fillCount), GL_UNSIGNED_INT, nullptr);
    }

    if (m_borderIndexBuffer.isCreated() && m_borderIndexCount > 0U) {
        m_borderIndexBuffer.bind();
        m_program->setUniformValue(m_colorUniform, palette::Border);
        glLineWidth(1.0F);
        for (int copy = copies.first; copy <= copies.last; ++copy) {
            m_program->setUniformValue(m_worldOffsetUniform,
                                       static_cast<float>(copy * MapProjection::WorldWidthDegrees));

            glDrawElements(GL_LINES, static_cast<GLsizei>(m_borderIndexCount), GL_UNSIGNED_INT,
                           nullptr);
        }
    }
    m_program->setUniformValue(m_worldOffsetUniform, 0.0F);
    m_program->release();
}

void EarthMapGpuRenderer::cleanup() noexcept {
    m_mapVertexArray.destroy();
    m_oceanVertexArray.destroy();
    m_vertexBuffer.destroy();
    m_mercatorIndexBuffer.destroy();
    m_sphereIndexBuffer.destroy();
    m_borderIndexBuffer.destroy();
    m_oceanVertexBuffer.destroy();
    m_program.reset();
    m_mercatorIndexCount = 0U;
    m_sphereIndexCount = 0U;
    m_borderIndexCount = 0U;
    m_oceanVertexCount = 0U;
    m_shaderValid = false;
    m_pendingUpload = m_mesh != nullptr && !m_mesh->empty();
}

bool EarthMapGpuRenderer::isShaderValid() const noexcept {
    return m_shaderValid;
}

bool EarthMapGpuRenderer::hasUploadedMesh() const noexcept {
    return m_mesh != nullptr && !m_pendingUpload;
}

} // namespace lar::map
