#pragma once

/**
 * @file plane_scene_widget.h
 * @brief Interactive OpenGL chase/orbit view for the centered Plane simulation.
 */

#include "viewer/lar_geodesic_geometry.h"
#include "viewer/plane/cubemap_catalog.h"
#include "viewer/plane/plane_orbit_camera.h"
#include "viewer/plane/plane_scene_renderer.h"
#include "viewer/plane/plane_surface_state.h"
#include "viewer/plane/plane_terrain_patch.h"
#include "viewer/viewport/lar_scene_state.h"

#include <QMetaObject>
#include <QOpenGLWidget>
#include <QThread>

#include <memory>
#include <optional>

class QMouseEvent;
class QWheelEvent;
class PlaneTerrainWorker;

/** @brief Loads packaged Plane assets and renders one orbit-controlled simulation canvas. */
class PlaneSceneWidget final : public QOpenGLWidget {
    Q_OBJECT

  public:
    explicit PlaneSceneWidget(QString packageDirectory, QWidget *parent = nullptr);
    ~PlaneSceneWidget() override;

    void setSceneState(const LarSceneState &state);
    void clearScene();
    void resetCamera();
    bool selectNextSkybox();
    void setSurfaceVisible(bool visible);
    /** @brief Enables DTED terrain, retaining flat ground as an unavailable/loading fallback. */
    void setTerrainVisible(bool visible);

    /**
     * @brief Loads a user-selected `.gltf` or `.glb` model.
     *
     * @details The current model is kept when the selected file cannot be read. The model is
     * uploaded to the OpenGL context on the next paint pass.
     *
     * @param[in] path Path of the `.gltf` or `.glb` file to load.
     *
     * @return True when the model was parsed and accepted; otherwise false.
     */
    bool loadModelFromFile(const QString &path);

    [[nodiscard]] int skyboxCount() const noexcept {
        return m_skyboxes.count();
    }
    [[nodiscard]] int skyboxIndex() const noexcept {
        return m_skyboxIndex;
    }
    [[nodiscard]] QString skyboxName() const {
        return m_skyboxes.displayName(m_skyboxIndex);
    }
    [[nodiscard]] const PlaneOrbitCamera &orbitCamera() const noexcept {
        return m_orbitCamera;
    }
    [[nodiscard]] bool rendererReady() const noexcept {
        return m_renderer.ready();
    }
    [[nodiscard]] bool surfaceVisible() const noexcept {
        return m_surfaceVisible;
    }
    /** @brief Returns whether terrain display was requested by the user. */
    [[nodiscard]] bool terrainVisible() const noexcept {
        return m_terrainVisible;
    }
    /** @brief Returns whether a readable DTED0 root was discovered at construction. */
    [[nodiscard]] bool terrainAvailable() const noexcept {
        return m_terrainAvailable;
    }
    /** @brief Returns whether the compact DTED-aligned water mask passed validation. */
    [[nodiscard]] bool terrainWaterMaskAvailable() const noexcept {
        return m_terrainWaterMaskAvailable;
    }
    /** @brief Returns whether an immutable terrain patch is ready for GPU upload. */
    [[nodiscard]] bool terrainPatchReady() const noexcept {
        return m_terrainPatch != nullptr && !m_terrainPatch->empty();
    }
    [[nodiscard]] const PlaneSurfaceState &surfaceState() const noexcept {
        return m_surfaceState;
    }

  public slots:
    void cleanupGL();

  signals:
    void frameRendered();
    void diagnosticRaised(const QString &message);
    void skyboxSelectionChanged(int index, int count, const QString &name);
    void surfaceVisibilityChanged(bool visible);
    /** @brief Notifies controls when optional DTED terrain visibility changes. */
    void terrainVisibilityChanged(bool visible);
    /** @brief Notifies tests and controls when terrain preparation changes readiness. */
    void terrainPatchChanged(bool ready);

  protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

  private:
    bool loadSkybox(int index);
    void refreshSurfaceState();
    void setDiagnostic(const QString &message);
    [[nodiscard]] double headingRadians() const noexcept;
    void initializeTerrainSource();
    void startTerrainWorker();
    void requestTerrainIfNeeded(bool force = false);
    void updateTerrainPlacement();
    void completeTerrainPatch(quint64 revision, PlaneTerrainPatchPtr patch, const QString &message);
    void stopTerrainWorker() noexcept;

    QString m_packageDirectory;
    CubemapCatalog m_skyboxes;
    PlaneSceneRenderer m_renderer;
    std::shared_ptr<const PlaneModelMesh> m_model;
    PlaneOrbitCamera m_orbitCamera;
    PlaneSurfaceState m_surfaceState;
    std::optional<GeoCoordinateRadians> m_surfaceGroundOrigin;
    std::optional<GeoCoordinateRadians> m_terrainRequestAnchor;
    std::optional<GeoCoordinateRadians> m_failedTerrainAnchor;
    PlaneTerrainPatchPtr m_terrainPatch;
    QThread m_terrainThread;
    PlaneTerrainWorker *m_terrainWorker = nullptr;
    LarSceneState m_scene;
    QPointF m_lastMousePosition;
    QString m_diagnostic;
    QString m_terrainRootDirectory;
    QString m_terrainWaterMaskFile;
    QString m_terrainWaterMaskError;
    QMetaObject::Connection m_contextConnection;
    quint64 m_terrainRevision = 0;
    double m_pendingTerrainHalfExtent = 0.0;
    double m_pendingTerrainScale = 0.0;
    double m_failedTerrainHalfExtent = 0.0;
    int m_skyboxIndex = 0;
    double m_surfaceMetersPerSceneUnit = PlaneAircraftScale::DefaultMetersPerSceneUnit;
    bool m_dragging = false;
    bool m_attitudeComplete = false;
    bool m_surfaceVisible = false;
    bool m_terrainVisible = false;
    bool m_terrainAvailable = false;
    bool m_terrainWaterMaskAvailable = false;
    bool m_terrainPending = false;
};
