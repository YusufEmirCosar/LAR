#include "viewer/plane/plane_terrain_gpu_layer.h"

#include "viewer/plane/plane_terrain_shaders.h"

#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>

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
    bool validWaterDepth = patch.vertices.size() % PlaneTerrainVertexStrideFloats == 0U;
    for (std::size_t offset = 0; validWaterDepth && offset < patch.vertices.size();
         offset += PlaneTerrainVertexStrideFloats) {
        validWaterDepth = patch.vertices[offset + 6U] >= 0.0F;
    }
    return finiteVertices && validWaterDepth && patch.landMask.valid() &&
           patch.sampleValidity.size() == patch.vertexCount() &&
           std::isfinite(patch.minimumElevationMeters) &&
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
    m_uploadPending = true;
}

bool PlaneTerrainGpuLayer::compileProgram(QString *errorMessage) {
    m_program = std::make_unique<QOpenGLShaderProgram>();
    bool vertex = m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                     plane::terrain::shaders::VertexLegacy);
    bool fragment = m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                       plane::terrain::shaders::FragmentLegacy);
    m_program->bindAttributeLocation("aPosition", 0);
    m_program->bindAttributeLocation("aNormal", 1);
    m_program->bindAttributeLocation("aWaterDepth", 2);
    bool linked = m_program->link();
    if (!vertex || !fragment || !linked) {
        m_program->removeAllShaders();
        vertex = m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                    plane::terrain::shaders::VertexCore);
        fragment = m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                      plane::terrain::shaders::FragmentCore);
        m_program->bindAttributeLocation("aPosition", 0);
        m_program->bindAttributeLocation("aNormal", 1);
        m_program->bindAttributeLocation("aWaterDepth", 2);
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
        m_landMaskTexture.reset();
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
    m_program->setAttributeBuffer(2, GL_FLOAT, 6 * static_cast<int>(sizeof(float)), 1,
                                  VertexStrideBytes);
    m_program->release();
    m_landMaskTexture.reset();
    const bool mixed = m_patch->landMask.coverage == PlaneLandCoverage::Mixed;
    const int maskResolution = mixed ? m_patch->landMask.resolution : 1;
    const unsigned char uniformTexel =
        m_patch->landMask.coverage == PlaneLandCoverage::AllLand ? 255U : 0U;
    const unsigned char *maskData = mixed ? m_patch->landMask.texels.data() : &uniformTexel;
    m_landMaskTexture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    m_landMaskTexture->setSize(maskResolution, maskResolution);
    m_landMaskTexture->setFormat(QOpenGLTexture::R8_UNorm);
    m_landMaskTexture->setMipLevels(1);
    m_landMaskTexture->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    m_landMaskTexture->setData(0, QOpenGLTexture::Red, QOpenGLTexture::UInt8, maskData);
    m_landMaskTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    m_landMaskTexture->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    if (!m_landMaskTexture->isCreated() || !m_landMaskTexture->isStorageAllocated()) {
        m_landMaskTexture.reset();
        setError(errorMessage, QStringLiteral("The Plane vector land mask could not be uploaded."));
        return false;
    }
    m_indexCount = static_cast<int>(m_patch->indices.size());
    m_uploadPending = false;
    return true;
}

bool PlaneTerrainGpuLayer::draw(const QMatrix4x4 &view, const QMatrix4x4 &projection,
                                const QVector2D &offsetXZ, const QVector2D &scaleXZ,
                                float aircraftAltitudeScene, QString *errorMessage) {
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
    m_program->setUniformValue(
        "uPatchHalfExtentScene",
        static_cast<float>(m_patch->halfExtentMeters / m_patch->metersPerSceneUnit));
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
    constexpr int LandMaskTextureUnit = 2;
    const bool mixed = m_patch->landMask.coverage == PlaneLandCoverage::Mixed;
    m_program->setUniformValue("uLandMask", LandMaskTextureUnit);
    m_program->setUniformValue("uUseLandMask", mixed ? 1.0F : 0.0F);
    m_program->setUniformValue(
        "uUniformLand", m_patch->landMask.coverage == PlaneLandCoverage::AllLand ? 1.0F : 0.0F);
    m_landMaskTexture->bind(LandMaskTextureUnit);
    const auto drawPass = [this](bool water) {
        m_program->setUniformValue("uRenderWater", water ? 1.0F : 0.0F);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, nullptr);
    };
    if (m_patch->landMask.coverage != PlaneLandCoverage::AllWater) {
        drawPass(false);
    }
    if (m_patch->landMask.coverage != PlaneLandCoverage::AllLand) {
        drawPass(true);
    }
    m_landMaskTexture->release(LandMaskTextureUnit);
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

std::optional<float>
PlaneTerrainGpuLayer::groundHeightAt(const QVector2D &currentXZ, const QVector2D &offsetXZ,
                                     const QVector2D &scaleXZ,
                                     float aircraftAltitudeScene) const noexcept {
    if (!hasRenderableGeometry() || !std::isfinite(currentXZ.x()) ||
        !std::isfinite(currentXZ.y()) || !std::isfinite(offsetXZ.x()) ||
        !std::isfinite(offsetXZ.y()) || !std::isfinite(scaleXZ.x()) ||
        !std::isfinite(scaleXZ.y()) || std::abs(scaleXZ.x()) <= 1.0e-6F ||
        std::abs(scaleXZ.y()) <= 1.0e-6F || !std::isfinite(aircraftAltitudeScene) ||
        m_patch->resolution < 2 || m_patch->sampleValidity.size() != m_patch->vertexCount()) {
        return std::nullopt;
    }

    const double patchX = (static_cast<double>(currentXZ.x()) - offsetXZ.x()) / scaleXZ.x();
    const double patchZ = (static_cast<double>(currentXZ.y()) - offsetXZ.y()) / scaleXZ.y();
    const double eastMeters = patchX * m_patch->metersPerSceneUnit;
    const double northMeters = -patchZ * m_patch->metersPerSceneUnit;
    if (eastMeters < -m_patch->halfExtentMeters || eastMeters > m_patch->halfExtentMeters ||
        northMeters < -m_patch->halfExtentMeters || northMeters > m_patch->halfExtentMeters) {
        return std::nullopt;
    }
    if (!m_patch->landMask.landAtLocal(eastMeters, northMeters)) {
        return -aircraftAltitudeScene;
    }

    const double scale =
        static_cast<double>(m_patch->resolution - 1) / (2.0 * m_patch->halfExtentMeters);
    const double columnPosition = (eastMeters + m_patch->halfExtentMeters) * scale;
    const double rowPosition = (northMeters + m_patch->halfExtentMeters) * scale;
    const int column0 =
        std::clamp(static_cast<int>(std::floor(columnPosition)), 0, m_patch->resolution - 1);
    const int row0 =
        std::clamp(static_cast<int>(std::floor(rowPosition)), 0, m_patch->resolution - 1);
    const int column1 = std::min(column0 + 1, m_patch->resolution - 1);
    const int row1 = std::min(row0 + 1, m_patch->resolution - 1);
    const auto vertex = [this](int row, int column) {
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(m_patch->resolution) +
               static_cast<std::size_t>(column);
    };
    const std::size_t southWest = vertex(row0, column0);
    const std::size_t southEast = vertex(row0, column1);
    const std::size_t northWest = vertex(row1, column0);
    const std::size_t northEast = vertex(row1, column1);
    for (const std::size_t sample : {southWest, southEast, northWest, northEast}) {
        if (m_patch->sampleValidity[sample] == 0U) {
            return std::nullopt;
        }
    }
    const auto height = [this](std::size_t sample) {
        return static_cast<double>(m_patch->vertices[sample * PlaneTerrainVertexStrideFloats + 1U]);
    };
    const double columnFraction = columnPosition - static_cast<double>(column0);
    const double rowFraction = rowPosition - static_cast<double>(row0);
    const double south =
        height(southWest) * (1.0 - columnFraction) + height(southEast) * columnFraction;
    const double north =
        height(northWest) * (1.0 - columnFraction) + height(northEast) * columnFraction;
    return static_cast<float>(south * (1.0 - rowFraction) + north * rowFraction) -
           aircraftAltitudeScene;
}

void PlaneTerrainGpuLayer::cleanup() noexcept {
    m_ready = false;
    m_indexCount = 0;
    m_landMaskTexture.reset();
    for (QOpenGLBuffer *buffer : {&m_vertices, &m_indices}) {
        if (buffer->isCreated()) {
            buffer->destroy();
        }
    }
    if (m_array.isCreated()) {
        m_array.destroy();
    }
    m_program.reset();
    m_uploadPending = true;
}
