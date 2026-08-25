#pragma once

/**
 * @file earth_map_gpu_renderer.h
 * @brief OpenGL resource owner and draw path for the packaged world map.
 */

#include "viewer/map/map_camera.h"
#include "viewer/map/map_mesh.h"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QString>

#include <cstddef>
#include <memory>

namespace lar::map {

/**
 * @brief Uploads immutable map meshes and renders Mercator or globe geometry.
 */
class EarthMapGpuRenderer final : protected QOpenGLFunctions {
  public:
    EarthMapGpuRenderer();
    ~EarthMapGpuRenderer();

    EarthMapGpuRenderer(const EarthMapGpuRenderer &other) = delete;
    EarthMapGpuRenderer &operator=(const EarthMapGpuRenderer &other) = delete;

    bool setMesh(std::shared_ptr<const MapMesh> mesh, QString *errorMessage = nullptr);
    bool initialize(QString *errorMessage = nullptr);
    bool uploadPending(QString *errorMessage = nullptr);
    void draw(const MapCamera &camera, int width, int height);
    void cleanup() noexcept;

    [[nodiscard]] bool isShaderValid() const noexcept;
    [[nodiscard]] bool hasUploadedMesh() const noexcept;

  private:
    bool compileProgram(QString *errorMessage);
    bool cacheProgramInputs(QString *errorMessage);
    bool createOceanDisc(QString *errorMessage);
    bool uploadIndexBuffer(QOpenGLBuffer &buffer, const std::vector<std::uint32_t> &indices,
                           const QString &label, QString *errorMessage);
    QOpenGLBuffer &fillIndexBuffer(MapPresentation presentation);
    [[nodiscard]] std::size_t fillIndexCount(MapPresentation presentation) const noexcept;

    std::unique_ptr<QOpenGLShaderProgram> m_program;
    QOpenGLBuffer m_vertexBuffer;
    QOpenGLBuffer m_mercatorIndexBuffer{QOpenGLBuffer::IndexBuffer};
    QOpenGLBuffer m_sphereIndexBuffer{QOpenGLBuffer::IndexBuffer};
    QOpenGLBuffer m_borderIndexBuffer{QOpenGLBuffer::IndexBuffer};
    QOpenGLBuffer m_oceanVertexBuffer;
    QOpenGLVertexArrayObject m_mapVertexArray;
    QOpenGLVertexArrayObject m_oceanVertexArray;
    std::shared_ptr<const MapMesh> m_mesh;
    std::size_t m_mercatorIndexCount = 0U;
    std::size_t m_sphereIndexCount = 0U;
    std::size_t m_borderIndexCount = 0U;
    std::size_t m_oceanVertexCount = 0U;
    bool m_pendingUpload = false;
    bool m_shaderValid = false;

    int m_positionAttribute = -1;
    int m_sphereUniform = -1;
    int m_screenSpaceUniform = -1;
    int m_worldOffsetUniform = -1;
    int m_sphereCenterHighUniform = -1;
    int m_sphereCenterLowUniform = -1;
    int m_sphereLatitudeSinCosUniform = -1;
    int m_bearingUniform = -1;
    int m_mercatorCenterUniform = -1;
    int m_projectionUniform = -1;
    int m_colorUniform = -1;
    int m_keepBackUniform = -1;
};

} // namespace lar::map
