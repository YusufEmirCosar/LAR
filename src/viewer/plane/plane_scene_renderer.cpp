
#include "viewer/plane/plane_scene_renderer.h"

#include "viewer/plane/plane_shaders.h"

#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

constexpr int VertexStrideFloats = static_cast<int>(PlaneModelVertexStrideFloats);
constexpr int VertexStrideBytes = VertexStrideFloats * static_cast<int>(sizeof(float));
void setError(QString *destination, const QString &message) {
    if (destination != nullptr) {
        *destination = message;
    }
}

QVector4D visibleAircraftColor(const QVector4D &source) noexcept {
    const auto lift = [](float component) {
        return std::clamp(0.48F + 0.42F * std::sqrt(std::clamp(component, 0.0F, 1.0F)), 0.0F, 1.0F);
    };
    return {lift(source.x()), lift(source.y()), lift(source.z()), source.w()};
}

QOpenGLTexture::WrapMode textureWrapMode(PlaneTextureWrap wrap) noexcept {
    switch (wrap) {
    case PlaneTextureWrap::ClampToEdge:
        return QOpenGLTexture::ClampToEdge;
    case PlaneTextureWrap::MirroredRepeat:
        return QOpenGLTexture::MirroredRepeat;
    case PlaneTextureWrap::Repeat:
        return QOpenGLTexture::Repeat;
    }
    return QOpenGLTexture::Repeat;
}

QOpenGLTexture::Filter textureMinificationFilter(PlaneTextureMinFilter filter) noexcept {
    switch (filter) {
    case PlaneTextureMinFilter::Nearest:
        return QOpenGLTexture::Nearest;
    case PlaneTextureMinFilter::Linear:
        return QOpenGLTexture::Linear;
    case PlaneTextureMinFilter::NearestMipmapNearest:
        return QOpenGLTexture::NearestMipMapNearest;
    case PlaneTextureMinFilter::LinearMipmapNearest:
        return QOpenGLTexture::LinearMipMapNearest;
    case PlaneTextureMinFilter::NearestMipmapLinear:
        return QOpenGLTexture::NearestMipMapLinear;
    case PlaneTextureMinFilter::LinearMipmapLinear:
        return QOpenGLTexture::LinearMipMapLinear;
    }
    return QOpenGLTexture::LinearMipMapLinear;
}

QOpenGLTexture::Filter textureMagnificationFilter(PlaneTextureMagFilter filter) noexcept {
    return filter == PlaneTextureMagFilter::Nearest ? QOpenGLTexture::Nearest
                                                    : QOpenGLTexture::Linear;
}

bool usesMipmaps(PlaneTextureMinFilter filter) noexcept {
    return filter != PlaneTextureMinFilter::Nearest && filter != PlaneTextureMinFilter::Linear;
}

int mipLevelCount(const QSize &size) noexcept {
    int levels = 1;
    for (int extent = std::max(size.width(), size.height()); extent > 1; extent /= 2) {
        ++levels;
    }
    return levels;
}

bool compileProgram(QOpenGLShaderProgram &program, const char *legacyVertex,
                    const char *legacyFragment, const char *coreVertex, const char *coreFragment,
                    bool normalAttribute, QString *errorMessage) {
    bool vertexCompiled = program.addShaderFromSourceCode(QOpenGLShader::Vertex, legacyVertex);
    bool fragmentCompiled =
        program.addShaderFromSourceCode(QOpenGLShader::Fragment, legacyFragment);
    program.bindAttributeLocation("aPosition", 0);
    if (normalAttribute) {
        program.bindAttributeLocation("aNormal", 1);
        program.bindAttributeLocation("aTexCoord", 2);
    }
    bool linked = program.link();
    if (!vertexCompiled || !fragmentCompiled || !linked) {
        program.removeAllShaders();
        vertexCompiled = program.addShaderFromSourceCode(QOpenGLShader::Vertex, coreVertex);
        fragmentCompiled = program.addShaderFromSourceCode(QOpenGLShader::Fragment, coreFragment);
        program.bindAttributeLocation("aPosition", 0);
        if (normalAttribute) {
            program.bindAttributeLocation("aNormal", 1);
            program.bindAttributeLocation("aTexCoord", 2);
        }
        linked = program.link();
    }
    if (!vertexCompiled || !fragmentCompiled || !linked) {
        setError(errorMessage,
                 QStringLiteral("Plane shader initialization failed: %1").arg(program.log()));
        return false;
    }
    return true;
}

} // namespace

PlaneSceneRenderer::PlaneSceneRenderer() = default;

PlaneSceneRenderer::~PlaneSceneRenderer() {
    m_modelTextures.clear();
    m_whiteModelTexture.reset();
    m_cubemap.reset();
    m_skyProgram.reset();
    m_meshProgram.reset();
}

void PlaneSceneRenderer::setModel(std::shared_ptr<const PlaneModelMesh> model) {
    m_model = std::move(model);
    m_modelPending = m_model != nullptr;
}

void PlaneSceneRenderer::setSurfaceState(const PlaneSurfaceState &state) noexcept {
    m_surfaceState = state;
}

void PlaneSceneRenderer::setSurfaceVisible(bool visible) noexcept {
    m_surfaceVisible = visible;
}

void PlaneSceneRenderer::setTerrainPatch(PlaneTerrainPatchPtr patch) noexcept {
    m_terrainLayer.setPatch(std::move(patch));
}

void PlaneSceneRenderer::setTerrainVisible(bool visible) noexcept {
    m_terrainVisible = visible;
}

void PlaneSceneRenderer::setTerrainPlacement(const QVector2D &offsetXZ,
                                             const QVector2D &scaleXZ,
                                             float aircraftAltitudeScene) noexcept {
    m_terrainOffsetXZ = offsetXZ;
    m_terrainScaleXZ = scaleXZ;
    m_aircraftAltitudeScene = aircraftAltitudeScene;
}

void PlaneSceneRenderer::setTerrainPlacement(const QVector2D &offsetXZ,
                                             float aircraftAltitudeScene) noexcept {
    setTerrainPlacement(offsetXZ, {1.0F, 1.0F}, aircraftAltitudeScene);
}

bool PlaneSceneRenderer::setSkybox(const CubemapFaces &faces, QString *errorMessage) {
    const QSize size = faces.images[0].size();
    if (size.width() <= 0 || size.width() != size.height() || size.width() > 4096) {
        setError(errorMessage, QStringLiteral("The selected skybox face dimensions are invalid."));
        return false;
    }
    for (const QImage &image : faces.images) {
        if (image.isNull() || image.size() != size || image.format() != QImage::Format_RGBA8888) {
            setError(errorMessage, QStringLiteral("The selected skybox faces do not match."));
            return false;
        }
    }
    m_pendingSkybox = faces;
    m_skyboxPending = true;
    return true;
}

bool PlaneSceneRenderer::compilePrograms(QString *errorMessage) {
    m_meshProgram = std::make_unique<QOpenGLShaderProgram>();
    m_skyProgram = std::make_unique<QOpenGLShaderProgram>();
    return compileProgram(*m_meshProgram, plane::shaders::MeshVertexLegacy,
                          plane::shaders::MeshFragmentLegacy, plane::shaders::MeshVertexCore,
                          plane::shaders::MeshFragmentCore, true, errorMessage) &&
           compileProgram(*m_skyProgram, plane::shaders::SkyVertexLegacy,
                          plane::shaders::SkyFragmentLegacy, plane::shaders::SkyVertexCore,
                          plane::shaders::SkyFragmentCore, false, errorMessage);
}

bool PlaneSceneRenderer::uploadInterleaved(const std::vector<float> &vertices,
                                           const std::vector<std::uint32_t> &indices,
                                           QOpenGLBuffer &vertexBuffer, QOpenGLBuffer &indexBuffer,
                                           QOpenGLVertexArrayObject &vertexArray,
                                           QString *errorMessage) {
    if (vertices.empty() || vertices.size() % PlaneModelVertexStrideFloats != 0U ||
        vertices.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) / sizeof(float) ||
        indices.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) / sizeof(std::uint32_t)) {
        setError(errorMessage, QStringLiteral("Plane geometry exceeds GPU buffer limits."));
        return false;
    }
    if ((!vertexBuffer.isCreated() && !vertexBuffer.create()) ||
        (!vertexArray.isCreated() && !vertexArray.create()) ||
        (!indices.empty() && !indexBuffer.isCreated() && !indexBuffer.create())) {
        setError(errorMessage, QStringLiteral("Plane GPU geometry could not be created."));
        return false;
    }
    m_meshProgram->bind();
    QOpenGLVertexArrayObject::Binder binder(&vertexArray);
    vertexBuffer.bind();
    vertexBuffer.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(float)));
    m_meshProgram->enableAttributeArray(0);
    m_meshProgram->enableAttributeArray(1);
    m_meshProgram->enableAttributeArray(2);
    m_meshProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, VertexStrideBytes);
    m_meshProgram->setAttributeBuffer(1, GL_FLOAT, 3 * static_cast<int>(sizeof(float)), 3,
                                      VertexStrideBytes);
    m_meshProgram->setAttributeBuffer(2, GL_FLOAT, 6 * static_cast<int>(sizeof(float)), 2,
                                      VertexStrideBytes);
    if (!indices.empty()) {
        indexBuffer.bind();
        indexBuffer.allocate(indices.data(),
                             static_cast<int>(indices.size() * sizeof(std::uint32_t)));
    }
    m_meshProgram->release();
    return true;
}

bool PlaneSceneRenderer::createStaticGeometry(QString *errorMessage) {
    static const float CubeVertices[] = {
        -1, 1,  -1, -1, -1, -1, 1,  -1, -1, 1,  -1, -1, 1,  1,  -1, -1, 1,  -1, -1, -1, 1, -1,
        -1, -1, -1, 1,  -1, -1, 1,  -1, -1, 1,  1,  -1, -1, 1,  1,  -1, -1, 1,  -1, 1,  1, 1,
        1,  1,  1,  1,  1,  1,  -1, 1,  -1, -1, -1, -1, 1,  -1, 1,  1,  1,  1,  1,  1,  1, 1,
        1,  -1, 1,  -1, -1, 1,  -1, 1,  -1, 1,  1,  -1, 1,  1,  1,  1,  1,  1,  -1, 1,  1, -1,
        1,  -1, -1, -1, -1, -1, -1, 1,  1,  -1, -1, 1,  -1, -1, -1, -1, 1,  1,  -1, 1};
    if ((!m_skyVertices.isCreated() && !m_skyVertices.create()) ||
        (!m_skyArray.isCreated() && !m_skyArray.create())) {
        setError(errorMessage, QStringLiteral("The skybox GPU geometry could not be created."));
        return false;
    }
    m_skyProgram->bind();
    QOpenGLVertexArrayObject::Binder skyBinder(&m_skyArray);
    m_skyVertices.bind();
    m_skyVertices.allocate(CubeVertices, static_cast<int>(sizeof(CubeVertices)));
    m_skyProgram->enableAttributeArray(0);
    m_skyProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * static_cast<int>(sizeof(float)));
    m_skyProgram->release();
    return true;
}

bool PlaneSceneRenderer::uploadModel(QString *errorMessage) {
    if (m_model == nullptr || m_model->empty()) {
        setError(errorMessage,
                 QStringLiteral("The selected plane model has no renderable geometry."));
        return false;
    }
    if (!uploadInterleaved(m_model->vertices, m_model->indices, m_modelVertices, m_modelIndices,
                           m_modelArray, errorMessage)) {
        return false;
    }
    m_modelTextures.clear();
    m_whiteModelTexture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    m_whiteModelTexture->setSize(1, 1);
    m_whiteModelTexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
    m_whiteModelTexture->setMipLevels(1);
    m_whiteModelTexture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
    constexpr std::uint32_t WhitePixel = 0xFFFFFFFFU;
    m_whiteModelTexture->setData(0, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, &WhitePixel);
    m_whiteModelTexture->setMipLevelRange(0, 0);
    m_whiteModelTexture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    if (!m_whiteModelTexture->isCreated() || !m_whiteModelTexture->isStorageAllocated()) {
        setError(errorMessage, QStringLiteral("The fallback plane texture could not be uploaded."));
        return false;
    }
    m_modelTextures.reserve(m_model->textures.size());
    for (const PlaneModelTexture &source : m_model->textures) {
        if (source.image.isNull() || source.image.format() != QImage::Format_RGBA8888) {
            setError(
                errorMessage,
                QStringLiteral("The selected plane model contains an invalid decoded texture."));
            return false;
        }
        auto texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        texture->setSize(source.image.width(), source.image.height());
        texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        const bool mipmapped = usesMipmaps(source.minFilter);
        const int mipLevels = mipmapped ? mipLevelCount(source.image.size()) : 1;
        texture->setMipLevels(mipLevels);
        texture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
        texture->setData(0, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, source.image.constBits());
        texture->setMipLevelRange(0, mipLevels - 1);
        texture->setWrapMode(QOpenGLTexture::DirectionS, textureWrapMode(source.wrapS));
        texture->setWrapMode(QOpenGLTexture::DirectionT, textureWrapMode(source.wrapT));
        texture->setMinificationFilter(textureMinificationFilter(source.minFilter));
        texture->setMagnificationFilter(textureMagnificationFilter(source.magFilter));
        if (mipmapped) {
            texture->generateMipMaps(0, false);
        }
        if (!texture->isCreated() || !texture->isStorageAllocated()) {
            setError(errorMessage,
                     QStringLiteral("A plane-model base-color texture could not be uploaded."));
            return false;
        }
        m_modelTextures.push_back(std::move(texture));
    }
    m_modelPending = false;
    return true;
}

bool PlaneSceneRenderer::uploadSkybox(QString *errorMessage) {
    if (!m_skyboxPending) {
        return true;
    }
    auto texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::TargetCubeMap);
    const int size = m_pendingSkybox.images[0].width();
    texture->setSize(size, size);
    texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
    texture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
    static constexpr QOpenGLTexture::CubeMapFace Faces[] = {
        QOpenGLTexture::CubeMapPositiveX, QOpenGLTexture::CubeMapNegativeX,
        QOpenGLTexture::CubeMapPositiveY, QOpenGLTexture::CubeMapNegativeY,
        QOpenGLTexture::CubeMapPositiveZ, QOpenGLTexture::CubeMapNegativeZ};
    for (int index = 0; index < 6; ++index) {
        texture->setData(0, 0, Faces[index], QOpenGLTexture::RGBA, QOpenGLTexture::UInt8,
                         m_pendingSkybox.images[static_cast<std::size_t>(index)].constBits());
    }
    texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    texture->setMinificationFilter(QOpenGLTexture::Linear);
    texture->setMagnificationFilter(QOpenGLTexture::Linear);
    if (!texture->isCreated() || !texture->isStorageAllocated()) {
        setError(errorMessage, QStringLiteral("The selected skybox could not be uploaded."));
        return false;
    }
    m_cubemap = std::move(texture);
    m_skyboxPending = false;
    return true;
}

bool PlaneSceneRenderer::initialize(QString *errorMessage) {
    initializeOpenGLFunctions();
    cleanup();
    if (!compilePrograms(errorMessage) || !createStaticGeometry(errorMessage) ||
        (m_modelPending && !uploadModel(errorMessage)) || !uploadSkybox(errorMessage) ||
        !m_surfaceLayer.initialize(errorMessage) || !m_terrainLayer.initialize(errorMessage)) {
        return false;
    }
    glEnable(GL_DEPTH_TEST);
    m_ready = true;
    return true;
}

void PlaneSceneRenderer::drawMesh(QOpenGLVertexArrayObject &vertexArray, QOpenGLBuffer &indexBuffer,
                                  const QMatrix4x4 &model, const QMatrix4x4 &view,
                                  const QMatrix4x4 &projection, const DrawRange &range, bool unlit,
                                  float pointSize) {
    if (range.indexCount == 0U ||
        range.indexCount > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        return;
    }
    m_meshProgram->bind();
    m_meshProgram->setUniformValue("uModel", model);
    m_meshProgram->setUniformValue("uView", view);
    m_meshProgram->setUniformValue("uProjection", projection);
    m_meshProgram->setUniformValue("uNormalMatrix", model.normalMatrix());
    m_meshProgram->setUniformValue("uColor", range.color);
    m_meshProgram->setUniformValue("uLightDirection", QVector3D(0.35F, 0.85F, 0.4F));
    m_meshProgram->setUniformValue("uUnlit", unlit);
    m_meshProgram->setUniformValue("uPointSize", pointSize);
    QOpenGLTexture *texture = nullptr;
    if (range.textureIndex >= 0 &&
        static_cast<std::size_t>(range.textureIndex) < m_modelTextures.size()) {
        texture = m_modelTextures[static_cast<std::size_t>(range.textureIndex)].get();
    }
    m_meshProgram->setUniformValue("uHasBaseColorTexture", texture != nullptr);
    constexpr int ModelTextureUnit = 1;
    m_meshProgram->setUniformValue("uBaseColorTexture", ModelTextureUnit);
    QOpenGLTexture *boundTexture = texture != nullptr ? texture : m_whiteModelTexture.get();
    boundTexture->bind(ModelTextureUnit);
    QOpenGLVertexArrayObject::Binder binder(&vertexArray);
    indexBuffer.bind();
    glDrawElements(range.primitive, static_cast<GLsizei>(range.indexCount), GL_UNSIGNED_INT,
                   reinterpret_cast<const void *>(range.firstIndex * sizeof(std::uint32_t)));
    boundTexture->release(ModelTextureUnit);
    m_meshProgram->release();
}

void PlaneSceneRenderer::drawSkybox(const QMatrix4x4 &view, const QMatrix4x4 &projection) {
    if (m_cubemap == nullptr) {
        return;
    }
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    m_skyProgram->bind();
    m_skyProgram->setUniformValue("uViewRotation", view.normalMatrix());
    m_skyProgram->setUniformValue("uProjection", projection);
    m_skyProgram->setUniformValue("uSkybox", 0);
    m_cubemap->bind(0);
    QOpenGLVertexArrayObject::Binder binder(&m_skyArray);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    m_cubemap->release();
    m_skyProgram->release();
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}

void PlaneSceneRenderer::drawAircraft(const QMatrix4x4 &view, const QMatrix4x4 &projection,
                                      const QQuaternion &attitude) {
    if (m_model == nullptr) {
        return;
    }
    QMatrix4x4 model;
    model.rotate(attitude);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    for (const PlaneModelDrawRange &source : m_model->draws) {
        if (source.doubleSided)
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);
        const QVector4D color = source.baseColorTexture >= 0
                                    ? source.baseColor
                                    : visibleAircraftColor(source.baseColor);
        drawMesh(
            m_modelArray, m_modelIndices, model, view, projection,
            {source.firstIndex, source.indexCount, color, source.baseColorTexture, GL_TRIANGLES});
    }
    glDisable(GL_CULL_FACE);
}

bool PlaneSceneRenderer::draw(const QMatrix4x4 &view, const QMatrix4x4 &projection,
                              const QQuaternion &aircraftAttitude, QString *errorMessage) {
    if (!m_ready) {
        setError(errorMessage, QStringLiteral("Plane GPU resources are not ready."));
        return false;
    }
    if ((m_modelPending && !uploadModel(errorMessage)) || !uploadSkybox(errorMessage)) {
        return false;
    }
    const QVector4D clearColor(0.08F, 0.1F, 0.14F, 1.0F);
    glClearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawSkybox(view, projection);
    bool terrainDrawn = false;
    if (m_terrainVisible) {
        if (!m_terrainLayer.draw(view, projection, m_terrainOffsetXZ, m_terrainScaleXZ,
                                 m_aircraftAltitudeScene, errorMessage)) {
            return false;
        }
        terrainDrawn = m_terrainLayer.hasRenderableGeometry();
    }
    // A terrain-enabled scene must not silently fall back to land-colored flat ground while the
    // asynchronous patch is loading. That transient surface made a near-shore target appear on
    // land in one frame and at sea after the patch arrived. Keep the tactical overlays available,
    // but only draw the flat ground when terrain is not the selected source.
    const bool drawFlatGround = !m_terrainVisible && !terrainDrawn;
    if (m_surfaceVisible || drawFlatGround) {
        PlaneSurfaceState surfaceState = m_surfaceState;
        if (terrainDrawn) {
            const std::optional<float> groundHeight =
                m_terrainLayer.centerGroundHeight(m_aircraftAltitudeScene);
            if (groundHeight) {
                surfaceState.surfaceHeight = *groundHeight;
            }
            if (surfaceState.targetVisible) {
                const std::optional<float> targetHeight = m_terrainLayer.groundHeightAt(
                    surfaceState.targetXZ, m_terrainOffsetXZ, m_terrainScaleXZ,
                    m_aircraftAltitudeScene);
                if (targetHeight) {
                    surfaceState.targetHeight = *targetHeight;
                }
            }
        }
        m_surfaceLayer.draw(surfaceState, view, projection, drawFlatGround, m_surfaceVisible);
    }
    drawAircraft(view, projection, aircraftAttitude);
    return true;
}

void PlaneSceneRenderer::cleanup() noexcept {
    m_terrainLayer.cleanup();
    m_surfaceLayer.cleanup();
    m_modelTextures.clear();
    m_whiteModelTexture.reset();
    m_cubemap.reset();
    for (QOpenGLBuffer *buffer : {&m_modelVertices, &m_modelIndices, &m_skyVertices}) {
        if (buffer->isCreated()) {
            buffer->destroy();
        }
    }
    for (QOpenGLVertexArrayObject *array : {&m_modelArray, &m_skyArray}) {
        if (array->isCreated()) {
            array->destroy();
        }
    }
    m_skyProgram.reset();
    m_meshProgram.reset();
    m_modelPending = m_model != nullptr;
    m_skyboxPending = !m_pendingSkybox.images[0].isNull();
    m_ready = false;
}
