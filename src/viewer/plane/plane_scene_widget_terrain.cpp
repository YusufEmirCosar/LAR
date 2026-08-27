/**
 * @file plane_scene_widget_terrain.cpp
 * @brief DTED worker lifecycle, patch recentering, and aircraft-relative placement.
 */

#include "viewer/plane/plane_scene_widget.h"

#include "domain/statefield.h"
#include "viewer/lar_projection.h"
#include "viewer/plane/plane_terrain_worker.h"
#include "viewer/terrain/dted_cell_reader.h"
#include "viewer/terrain/dted_tile_source.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>

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

QString defaultTerrainRootDirectory(const QString &packageDirectory) {
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

int terrainResolution(double halfExtentMeters, DtedLevel level) noexcept {
    int resolution = static_cast<int>(std::ceil((halfExtentMeters * 2.0) /
                                                dtedNominalPostSpacingMeters(level))) +
                     1;
    resolution = std::clamp(resolution, 49, 257);
    if (resolution % 2 == 0) {
        resolution += resolution < 257 ? 1 : -1;
    }
    return resolution;
}

std::optional<DtedCellKey> keyForCandidatePath(const QString &path, DtedLevel level) {
    static const QRegularExpression LongitudePattern(QStringLiteral("^([eEwW])(\\d{3})$"));
    static const QRegularExpression LatitudePattern(
        QStringLiteral("^([nNsS])(\\d{2})\\.[dD][tT]([012])$"));
    const QFileInfo fileInfo(path);
    const QRegularExpressionMatch longitudeMatch = LongitudePattern.match(fileInfo.dir().dirName());
    const QRegularExpressionMatch latitudeMatch = LatitudePattern.match(fileInfo.fileName());
    if (!longitudeMatch.hasMatch() || !latitudeMatch.hasMatch() ||
        latitudeMatch.captured(3).toInt() != dtedLevelNumber(level)) {
        return std::nullopt;
    }
    int longitude = longitudeMatch.captured(2).toInt();
    int latitude = latitudeMatch.captured(2).toInt();
    if (longitudeMatch.captured(1).compare(QStringLiteral("w"), Qt::CaseInsensitive) == 0) {
        longitude = -longitude;
    }
    if (latitudeMatch.captured(1).compare(QStringLiteral("s"), Qt::CaseInsensitive) == 0) {
        latitude = -latitude;
    }
    if (longitude < -180 || longitude > 179 || latitude < -90 || latitude > 89) {
        return std::nullopt;
    }
    return DtedCellKey{longitude, latitude};
}

QString validateTerrainDataset(const DtedDataset &dataset) {
    const DtedTileSource source(dataset.rootDirectory, dataset.level);
    if (!source.isAvailable()) {
        return QStringLiteral("The selected DTED folder is not a readable directory.");
    }
    const QString suffix = dtedFileSuffix(dataset.level);
    QDirIterator directoryIterator(dataset.rootDirectory, QDir::Dirs | QDir::NoDotAndDotDot,
                                   QDirIterator::NoIteratorFlags);
    constexpr int MaximumProbeDirectories = 1024;
    constexpr int MaximumProbeCandidates = 512;
    int directories = 0;
    int candidates = 0;
    QString firstFailure;
    while (directoryIterator.hasNext() && directories < MaximumProbeDirectories &&
           candidates < MaximumProbeCandidates) {
        const QString directoryPath = directoryIterator.next();
        ++directories;
        QDirIterator fileIterator(
            directoryPath,
            {QStringLiteral("*%1").arg(suffix), QStringLiteral("*%1").arg(suffix.toUpper())},
            QDir::Files | QDir::Readable, QDirIterator::NoIteratorFlags);
        while (fileIterator.hasNext() && candidates < MaximumProbeCandidates) {
            const QString candidatePath = fileIterator.next();
            ++candidates;
            const std::optional<DtedCellKey> key =
                keyForCandidatePath(candidatePath, dataset.level);
            if (!key) {
                continue;
            }
            const QString addressedPath = source.pathFor(*key);
            const QString candidateCanonical = QFileInfo(candidatePath).canonicalFilePath();
            const QString addressedCanonical = QFileInfo(addressedPath).canonicalFilePath();
            if (candidateCanonical.isEmpty() || candidateCanonical != addressedCanonical) {
                continue;
            }
            const DtedCellReadResult result = source.load(*key);
            if (result.succeeded()) {
                return {};
            }
            if (firstFailure.isEmpty()) {
                firstFailure = result.message;
            }
        }
    }
    if (!firstFailure.isEmpty()) {
        return firstFailure;
    }
    return QStringLiteral("No valid %1 tile was found under the selected folder. Expected "
                          "{e|w}DDD/{n|s}DD%2.")
        .arg(dtedLevelDisplayName(dataset.level), suffix);
}

} // namespace

void PlaneSceneWidget::initializeTerrainSource() {
    qRegisterMetaType<PlaneTerrainBuildRequest>("PlaneTerrainBuildRequest");
    qRegisterMetaType<PlaneTerrainPatchPtr>("PlaneTerrainPatchPtr");
    qRegisterMetaType<DtedLevel>("DtedLevel");
    m_terrainDataset = {defaultTerrainRootDirectory(m_packageDirectory), DtedLevel::Level0};
    m_terrainAvailable = QFileInfo(m_terrainDataset.rootDirectory).isDir();
}

void PlaneSceneWidget::startTerrainWorker() {
    if (m_terrainWorker != nullptr || !m_terrainAvailable) {
        return;
    }
    m_terrainThread.setObjectName(QStringLiteral("lar-plane-terrain-thread"));
    m_terrainWorker = new PlaneTerrainWorker(m_terrainDataset, m_mapAssetSource);
    m_terrainWorker->moveToThread(&m_terrainThread);
    connect(m_terrainWorker, &PlaneTerrainWorker::patchReady, this,
            &PlaneSceneWidget::completeTerrainPatch, Qt::QueuedConnection);
    m_terrainThread.start();
}

void PlaneSceneWidget::setTerrainVisible(bool visible) {
    if (visible && !m_terrainAvailable) {
        setDiagnostic(QStringLiteral(
            "DTED terrain is unavailable. Upload a DT1/DT2 folder or configure DTED0."));
        emit terrainVisibilityChanged(false);
        return;
    }
    if (visible && m_mapAssetSource == nullptr) {
        setDiagnostic(QStringLiteral(
            "The shared vector land map is unavailable; terrain cannot be classified."));
        emit terrainVisibilityChanged(false);
        return;
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

bool PlaneSceneWidget::loadTerrainFromDirectory(const QString &path, DtedLevel level) {
    if (path.trimmed().isEmpty()) {
        setDiagnostic(QStringLiteral("DTED terrain: No terrain folder was selected."));
        return false;
    }
    const DtedDataset candidate{QDir::cleanPath(QFileInfo(path).absoluteFilePath()), level};
    const QString validationError = validateTerrainDataset(candidate);
    if (!validationError.isEmpty()) {
        setDiagnostic(QStringLiteral("DTED terrain: %1").arg(validationError));
        return false;
    }

    const bool patchWasReady = terrainPatchReady();
    const bool availabilityChanged = !m_terrainAvailable;
    ++m_terrainRevision;
    stopTerrainWorker();
    m_terrainDataset = candidate;
    m_terrainAvailable = true;
    m_terrainRequestAnchor.reset();
    m_failedTerrainAnchor.reset();
    m_terrainPatch.reset();
    m_terrainPending = false;
    m_pendingTerrainHalfExtent = 0.0;
    m_pendingTerrainScale = 0.0;
    m_failedTerrainHalfExtent = 0.0;
    m_renderer.setTerrainPatch(nullptr);
    if (patchWasReady) {
        emit terrainPatchChanged(false);
    }
    setDiagnostic({});
    if (availabilityChanged) {
        emit terrainAvailabilityChanged(true);
    }
    emit terrainSourceChanged(level, candidate.rootDirectory);
    if (m_terrainVisible) {
        startTerrainWorker();
        requestTerrainIfNeeded(true);
    }
    update();
    return true;
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
    request.projectionOriginLatitudeRadians = anchor.latitude;
    request.halfExtentMeters = halfExtent;
    request.metersPerSceneUnit = scale;
    request.resolution = terrainResolution(halfExtent, m_terrainDataset.level);
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
        m_renderer.setTerrainPlacement({}, {1.0F, 1.0F}, 0.0F);
        return;
    }
    const double anchorPosition[3]{m_terrainPatch->anchorLatitudeRadians,
                                   m_terrainPatch->anchorLongitudeRadians, 0.0};
    const QPointF eastNorth = LarProjection::geographicToPlaneWorld(
        anchorPosition, m_scene.plane.location, 0.0, m_scene.plane.location[0], true);
    const double patchLatitude = std::isfinite(m_terrainPatch->projectionOriginLatitudeRadians)
                                     ? m_terrainPatch->projectionOriginLatitudeRadians
                                     : m_terrainPatch->anchorLatitudeRadians;
    const double currentCosineLatitude = std::max(0.01, std::cos(m_scene.plane.location[0]));
    const double patchCosineLatitude = std::max(0.01, std::cos(patchLatitude));
    const double scale = m_terrainPatch->metersPerSceneUnit;
    double altitudeMeters = m_terrainPatch->centerElevationMeters -
                            static_cast<double>(m_surfaceState.surfaceHeight) * scale;
    if (fieldAvailable(m_scene.availableFields, StateField::Location2) &&
        std::isfinite(m_scene.plane.location[2])) {
        altitudeMeters = m_scene.plane.location[2];
    }
    m_renderer.setTerrainPlacement(
        {static_cast<float>(eastNorth.x() / scale), static_cast<float>(-eastNorth.y() / scale)},
        {static_cast<float>(currentCosineLatitude / patchCosineLatitude), 1.0F},
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
        setDiagnostic(
            message.isEmpty()
                ? QStringLiteral("DTED terrain is unavailable here; terrain surface is hidden.")
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
