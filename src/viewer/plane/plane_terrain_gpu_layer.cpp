#include "viewer/plane/plane_terrain_gpu_layer.h"

#include "viewer/plane/plane_terrain_shaders.h"

#include <QOpenGLShaderProgram>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

constexpr int VertexStrideBytes = static_cast<int>(PlaneTerrainVertexStrideFloats * sizeof(float));

void setError(QString *destination, const QString &message) {
    if (destination != nullptr) {
        *destination = message;
    }
}

bool finitePatch(const PlaneTerrainPatch &patch) {
    const bool finiteVertices = std::all_of(patch.vertices.cbegin(), patch.vertices.cend(),
                                            [](float value) { return std::isfinite(value); });
    bool validWater = patch.vertices.size() % PlaneTerrainVertexStrideFloats == 0U;
    for (std::size_t offset = 0; validWater && offset < patch.vertices.size();
         offset += PlaneTerrainVertexStrideFloats) {
        const float water = patch.vertices[offset + 6U];
        const float depth = patch.vertices[offset + 7U];
        validWater = water >= 0.0F && water <= 1.0F && depth >= 0.0F;
    }
    return finiteVertices && validWater && std::isfinite(patch.minimumElevationMeters) &&
           std::isfinite(patch.maximumElevationMeters) &&
           std::isfinite(patch.maximumWaterDepthMeters) && patch.maximumWaterDepthMeters >= 0.0 &&
           patch.waterSampleCount <= patch.validSampleCount &&
           std::isfinite(patch.metersPerSceneUnit) && patch.metersPerSceneUnit > 0.0;
}

} // namespace

PlaneTerrainGpuLayer::PlaneTerrainGpuLayer() = default;

PlaneTerrainGpuLayer::~PlaneTerrainGpuLayer() = default;

void PlaneTerrainGpuLayer::setPatch(PlaneTerrainPatchPtr patch) noexcept {
    if (m_patch == patch) {
        return;
    }
    m_patch = std::move(patch);
    m_indexCount = 0;
    m_uploadPending = m_patch != nullptr;
}

bool PlaneTerrainGpuLayer::compileProgram(QString *errorMessage) {
    m_program = std::make_unique<QOpenGLShaderProgram>();
    bool vertex = m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                     plane::terrain::shaders::VertexLegacy);
    bool fragment = m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                       plane::terrain::shaders::FragmentLegacy);
    m_program->bindAttributeLocation("aPosition", 0);
    m_program->bindAttributeLocation("aNormal", 1);
    m_program->bindAttributeLocation("aWater", 2);
    bool linked = m_program->link();
    if (!vertex || !fragment || !linked) {
        m_program->removeAllShaders();
        vertex = m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                    plane::terrain::shaders::VertexCore);
        fragment = m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                      plane::terrain::shaders::FragmentCore);
        m_program->bindAttributeLocation("aPosition", 0);
        m_program->bindAttributeLocation("aNormal", 1);
        m_program->bindAttributeLocation("aWater", 2);
        linked = m_program->link();
    }
    if (!vertex || !fragment || !linked) {
        setError(
            errorMessage,
            QStringLiteral("Plane terrain shader initialization failed: %1").arg(m_program->log()));
        return false;
    }
    return true;
}

bool PlaneTerrainGpuLayer::createBuffers(QString *errorMessage) {
    if (!m_vertices.create() || !m_indices.create() || !m_array.create()) {
        setError(errorMessage, QStringLiteral("Plane terrain GPU buffers could not be created."));
        return false;
    }
    return true;
}

bool PlaneTerrainGpuLayer::initialize(QString *errorMessage) {
    initializeOpenGLFunctions();
    cleanup();
    if (!compileProgram(errorMessage) || !createBuffers(errorMessage)) {
        return false;
    }
    m_uploadPending = m_patch != nullptr;
    m_ready = true;
    return true;
}

bool PlaneTerrainGpuLayer::uploadPending(QString *errorMessage) {
    if (!m_uploadPending) {
        return true;
    }
    m_indexCount = 0;
    if (m_patch == nullptr || m_patch->empty()) {
        m_uploadPending = false;
        return true;
    }
    const std::size_t vertexCount = m_patch->vertexCount();
    const bool invalidSizes =
        m_patch->vertices.size() % PlaneTerrainVertexStrideFloats != 0U || vertexCount == 0U ||
        m_patch->vertices.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) / sizeof(float) ||
        m_patch->indices.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) / sizeof(std::uint32_t) ||
        m_patch->indices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max());
    const bool invalidIndex =
        std::any_of(m_patch->indices.cbegin(), m_patch->indices.cend(),
                    [vertexCount](std::uint32_t index) { return index >= vertexCount; });
    if (invalidSizes || invalidIndex || !finitePatch(*m_patch)) {
        setError(errorMessage, QStringLiteral("Plane terrain mesh exceeds GPU safety limits."));
        return false;
    }

    m_program->bind();
    QOpenGLVertexArrayObject::Binder binder(&m_array);
    m_vertices.bind();
    m_vertices.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_vertices.allocate(m_patch->vertices.data(),
                        static_cast<int>(m_patch->vertices.size() * sizeof(float)));
    m_indices.bind();
    m_indices.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_indices.allocate(m_patch->indices.data(),
                       static_cast<int>(m_patch->indices.size() * sizeof(std::uint32_t)));
    m_program->enableAttributeArray(0);
    m_program->enableAttributeArray(1);
    m_program->enableAttributeArray(2);
    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, VertexStrideBytes);
    m_program->setAttributeBuffer(1, GL_FLOAT, 3 * static_cast<int>(sizeof(float)), 3,
                                  VertexStrideBytes);
    m_program->setAttributeBuffer(2, GL_FLOAT, 6 * static_cast<int>(sizeof(float)), 2,
                                  VertexStrideBytes);
    m_program->release();
    m_indexCount = static_cast<int>(m_patch->indices.size());
    m_uploadPending = false;
    return true;
}

bool PlaneTerrainGpuLayer::draw(const QMatrix4x4 &view, const QMatrix4x4 &projection,
                                const QVector2D &offsetXZ, const QVector2D &scaleXZ,
                                float aircraftAltitudeScene,
                                QString *errorMessage) {
    if (!m_ready) {
        setError(errorMessage, QStringLiteral("Plane terrain GPU resources are not ready."));
        return false;
    }
    if (!uploadPending(errorMessage)) {
        return false;
    }
    if (!hasRenderableGeometry()) {
        return true;
    }
    m_program->bind();
    m_program->setUniformValue("uView", view);
    m_program->setUniformValue("uProjection", projection);
    m_program->setUniformValue("uOffsetXZ", offsetXZ);
    m_program->setUniformValue("uHorizontalScale", scaleXZ);
    m_program->setUniformValue("uAircraftAltitudeScene", aircraftAltitudeScene);
    m_program->setUniformValue("uMetersPerSceneUnit",
                               static_cast<float>(m_patch->metersPerSceneUnit));
    m_program->setUniformValue("uMinimumElevationMeters",
                               static_cast<float>(m_patch->minimumElevationMeters));
    m_program->setUniformValue("uMaximumElevationMeters",
                               static_cast<float>(m_patch->maximumElevationMeters));
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    QOpenGLVertexArrayObject::Binder binder(&m_array);
    m_indices.bind();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, nullptr);
    m_program->release();
    return true;
}

bool PlaneTerrainGpuLayer::draw(const QMatrix4x4 &view, const QMatrix4x4 &projection,
                                const QVector2D &offsetXZ, float aircraftAltitudeScene,
                                QString *errorMessage) {
    return draw(view, projection, offsetXZ, {1.0F, 1.0F}, aircraftAltitudeScene, errorMessage);
}

std::optional<float>
PlaneTerrainGpuLayer::centerGroundHeight(float aircraftAltitudeScene) const noexcept {
    if (!hasRenderableGeometry() || !std::isfinite(aircraftAltitudeScene)) {
        return std::nullopt;
    }
    return static_cast<float>(m_patch->centerElevationMeters / m_patch->metersPerSceneUnit) -
           aircraftAltitudeScene;
}

void PlaneTerrainGpuLayer::cleanup() noexcept {
    m_ready = false;
    m_indexCount = 0;
    for (QOpenGLBuffer *buffer : {&m_vertices, &m_indices}) {
        if (buffer->isCreated()) {
            buffer->destroy();
        }
    }
    if (m_array.isCreated()) {
        m_array.destroy();
    }
    m_program.reset();
    m_uploadPending = m_patch != nullptr;
}
