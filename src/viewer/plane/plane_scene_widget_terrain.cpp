/**
 * @file plane_scene_widget_terrain.cpp
 * @brief DTED worker lifecycle, patch recentering, and aircraft-relative placement.
 */

#include "viewer/plane/plane_scene_widget.h"

#include "domain/statefield.h"
#include "viewer/lar_projection.h"
#include "viewer/plane/plane_terrain_worker.h"
#include "viewer/terrain/dted_water_mask_source.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cmath>

namespace {

bool fieldAvailable(const QBitArray &fields, int field) noexcept {
    return field >= 0 && field < fields.size() && fields.testBit(field);
}

bool validGroundPosition(double latitude, double longitude) noexcept {
    constexpr double HalfPi = LarProjection::Pi * 0.5;
    return std::isfinite(latitude) && std::isfinite(longitude) && latitude >= -HalfPi &&
           latitude <= HalfPi && longitude >= -LarProjection::Pi && longitude <= LarProjection::Pi;
}

QString terrainRootDirectory(const QString &packageDirectory) {
    const QString overridePath = qEnvironmentVariable("LAR_DTED0_ROOT");
    if (!overridePath.isEmpty()) {
        return QDir::cleanPath(overridePath);
    }
    const QString packaged = QDir(packageDirectory).filePath(QStringLiteral("assets/DTED0"));
    if (QFileInfo(packaged).isDir()) {
        return QDir::cleanPath(packaged);
    }
#ifdef LAR_DTED0_SOURCE_DIR
    const QString source = QDir::cleanPath(QString::fromUtf8(LAR_DTED0_SOURCE_DIR));
    if (QFileInfo(source).isDir()) {
        return source;
    }
#endif
    return QDir::cleanPath(packaged);
}

QString terrainWaterMaskFile(const QString &packageDirectory) {
    const QString overridePath = qEnvironmentVariable("LAR_DTED0_WATER_MASK");
    if (!overridePath.isEmpty()) {
        return QDir::cleanPath(overridePath);
    }
    const QString packaged =
        QDir(packageDirectory).filePath(QStringLiteral("assets/water/dted0_water_mask.bin"));
    if (QFileInfo(packaged).isFile()) {
        return QDir::cleanPath(packaged);
    }
#ifdef LAR_DTED0_WATER_MASK_SOURCE_FILE
    const QString source = QDir::cleanPath(QString::fromUtf8(LAR_DTED0_WATER_MASK_SOURCE_FILE));
    if (QFileInfo(source).isFile()) {
        return source;
    }
#endif
    return QDir::cleanPath(packaged);
}

double distanceFromAircraftMeters(const GeoCoordinateRadians &anchor,
                                  const Plane &aircraft) noexcept {
    const double position[3]{anchor.latitude, anchor.longitude, 0.0};
    const QPointF eastNorth = LarProjection::geographicToPlaneWorld(
        position, aircraft.location, 0.0, aircraft.location[0], true);
    return std::hypot(eastNorth.x(), eastNorth.y());
}

double terrainHalfExtentMeters(const PlaneSurfaceState &surface) noexcept {
    const double requested =
        static_cast<double>(surface.surfaceHalfExtent) * surface.metersPerSceneUnit;
    return std::clamp(requested, 20'000.0, 60'000.0);
}

int terrainResolution(double halfExtentMeters) noexcept {
    constexpr double NominalPostSpacingMeters = 900.0;
    int resolution =
        static_cast<int>(std::ceil((halfExtentMeters * 2.0) / NominalPostSpacingMeters)) + 1;
    resolution = std::clamp(resolution, 49, 129);
    if (resolution % 2 == 0) {
        resolution += resolution < 129 ? 1 : -1;
    }
    return resolution;
}

} // namespace

void PlaneSceneWidget::initializeTerrainSource() {
    qRegisterMetaType<PlaneTerrainBuildRequest>("PlaneTerrainBuildRequest");
    qRegisterMetaType<PlaneTerrainPatchPtr>("PlaneTerrainPatchPtr");
    m_terrainRootDirectory = terrainRootDirectory(m_packageDirectory);
    m_terrainWaterMaskFile = terrainWaterMaskFile(m_packageDirectory);
    m_terrainAvailable = QFileInfo(m_terrainRootDirectory).isDir();
    const DtedWaterMaskSource waterMaskSource(m_terrainWaterMaskFile);
    m_terrainWaterMaskAvailable = waterMaskSource.isAvailable();
    m_terrainWaterMaskError = waterMaskSource.initializationError();
}

void PlaneSceneWidget::startTerrainWorker() {
    if (m_terrainWorker != nullptr || !m_terrainAvailable) {
        return;
    }
    m_terrainThread.setObjectName(QStringLiteral("lar-plane-terrain-thread"));
    m_terrainWorker = new PlaneTerrainWorker(m_terrainRootDirectory, m_terrainWaterMaskFile);
    m_terrainWorker->moveToThread(&m_terrainThread);
    connect(m_terrainWorker, &PlaneTerrainWorker::patchReady, this,
            &PlaneSceneWidget::completeTerrainPatch, Qt::QueuedConnection);
    m_terrainThread.start();
}

void PlaneSceneWidget::setTerrainVisible(bool visible) {
    if (visible && !m_terrainAvailable) {
        setDiagnostic(QStringLiteral(
            "DTED terrain is unavailable. Set LAR_DTED0_ROOT or place DTED0 under assets."));
        emit terrainVisibilityChanged(false);
        return;
    }
    if (visible && !m_terrainWaterMaskAvailable) {
        setDiagnostic(
            m_terrainWaterMaskError.isEmpty()
                ? QStringLiteral("DTED water mask is unavailable; ocean shading is disabled.")
                : m_terrainWaterMaskError);
    }
    if (m_terrainVisible == visible) {
        if (visible) {
            requestTerrainIfNeeded();
        }
        return;
    }
    m_terrainVisible = visible;
    m_orbitCamera.setGroundConstrained(m_surfaceVisible || visible);
    m_renderer.setTerrainVisible(visible);
    if (visible) {
        startTerrainWorker();
        requestTerrainIfNeeded(m_failedTerrainAnchor.has_value());
    }
    emit terrainVisibilityChanged(visible);
    update();
}

void PlaneSceneWidget::requestTerrainIfNeeded(bool force) {
    if (!m_terrainVisible || !m_terrainAvailable || m_terrainWorker == nullptr ||
        !m_scene.hasScene || !fieldAvailable(m_scene.availableFields, StateField::Location0) ||
        !fieldAvailable(m_scene.availableFields, StateField::Location1) ||
        !validGroundPosition(m_scene.plane.location[0], m_scene.plane.location[1])) {
        return;
    }
    const GeoCoordinateRadians anchor{m_scene.plane.location[0], m_scene.plane.location[1]};
    const double halfExtent = terrainHalfExtentMeters(m_surfaceState);
    const double scale = m_surfaceState.metersPerSceneUnit;
    const auto covers = [this, halfExtent, scale](const GeoCoordinateRadians &candidate,
                                                  double candidateHalfExtent,
                                                  double candidateScale) {
        return std::abs(candidateScale - scale) <= scale * 1.0e-6 &&
               candidateHalfExtent >= halfExtent * 0.9 &&
               distanceFromAircraftMeters(candidate, m_scene.plane) <= candidateHalfExtent * 0.35;
    };
    if (!force && m_terrainPatch &&
        covers({m_terrainPatch->anchorLatitudeRadians, m_terrainPatch->anchorLongitudeRadians},
               m_terrainPatch->halfExtentMeters, m_terrainPatch->metersPerSceneUnit)) {
        return;
    }
    if (!force && m_terrainPending && m_terrainRequestAnchor &&
        covers(*m_terrainRequestAnchor, m_pendingTerrainHalfExtent, m_pendingTerrainScale)) {
        return;
    }
    if (!force && m_failedTerrainAnchor &&
        distanceFromAircraftMeters(*m_failedTerrainAnchor, m_scene.plane) <=
            std::max(5000.0, m_failedTerrainHalfExtent * 0.35)) {
        return;
    }
    if (m_terrainPatch &&
        distanceFromAircraftMeters(
            {m_terrainPatch->anchorLatitudeRadians, m_terrainPatch->anchorLongitudeRadians},
            m_scene.plane) > m_terrainPatch->halfExtentMeters * 0.8) {
        m_terrainPatch.reset();
        m_renderer.setTerrainPatch(nullptr);
        emit terrainPatchChanged(false);
    }

    PlaneTerrainBuildRequest request;
    request.revision = ++m_terrainRevision;
    request.latitudeRadians = anchor.latitude;
    request.longitudeRadians = anchor.longitude;
    request.halfExtentMeters = halfExtent;
    request.metersPerSceneUnit = scale;
    request.resolution = terrainResolution(halfExtent);
    m_terrainRequestAnchor = anchor;
    m_pendingTerrainHalfExtent = halfExtent;
    m_pendingTerrainScale = scale;
    m_terrainPending = true;
    m_failedTerrainAnchor.reset();
    m_terrainWorker->submit(request);
}

void PlaneSceneWidget::updateTerrainPlacement() {
    if (!m_terrainPatch || !m_scene.hasScene ||
        !fieldAvailable(m_scene.availableFields, StateField::Location0) ||
        !fieldAvailable(m_scene.availableFields, StateField::Location1) ||
        !validGroundPosition(m_scene.plane.location[0], m_scene.plane.location[1])) {
        m_renderer.setTerrainPlacement({}, 0.0F);
        return;
    }
    const double anchorPosition[3]{m_terrainPatch->anchorLatitudeRadians,
                                   m_terrainPatch->anchorLongitudeRadians, 0.0};
    const QPointF eastNorth = LarProjection::geographicToPlaneWorld(
        anchorPosition, m_scene.plane.location, 0.0, m_scene.plane.location[0], true);
    const double scale = m_terrainPatch->metersPerSceneUnit;
    double altitudeMeters = m_terrainPatch->centerElevationMeters -
                            static_cast<double>(m_surfaceState.surfaceHeight) * scale;
    if (fieldAvailable(m_scene.availableFields, StateField::Location2) &&
        std::isfinite(m_scene.plane.location[2])) {
        altitudeMeters = m_scene.plane.location[2];
    }
    m_renderer.setTerrainPlacement(
        {static_cast<float>(eastNorth.x() / scale), static_cast<float>(-eastNorth.y() / scale)},
        static_cast<float>(altitudeMeters / scale));
}

void PlaneSceneWidget::completeTerrainPatch(quint64 revision, PlaneTerrainPatchPtr patch,
                                            const QString &message) {
    if (revision != m_terrainRevision) {
        return;
    }
    m_terrainPending = false;
    if (patch && !patch->empty()) {
        m_terrainPatch = std::move(patch);
        m_failedTerrainAnchor.reset();
        m_renderer.setTerrainPatch(m_terrainPatch);
        updateTerrainPlacement();
        if (m_diagnostic.startsWith(QStringLiteral("DTED terrain"))) {
            m_diagnostic.clear();
        }
        emit terrainPatchChanged(true);
    } else {
        m_terrainPatch.reset();
        m_renderer.setTerrainPatch(nullptr);
        m_failedTerrainAnchor = m_terrainRequestAnchor;
        m_failedTerrainHalfExtent = m_pendingTerrainHalfExtent;
        setDiagnostic(message.isEmpty()
                          ? QStringLiteral("DTED terrain is unavailable here; using flat ground.")
                          : QStringLiteral("DTED terrain: %1").arg(message));
        emit terrainPatchChanged(false);
    }
    update();
}

void PlaneSceneWidget::stopTerrainWorker() noexcept {
    if (m_terrainWorker == nullptr) {
        return;
    }
    m_terrainWorker->stop();
    m_terrainThread.quit();
    m_terrainThread.wait();
    delete m_terrainWorker;
    m_terrainWorker = nullptr;
}
