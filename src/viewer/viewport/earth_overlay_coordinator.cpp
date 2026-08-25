
#include "viewer/viewport/earth_overlay_coordinator.h"

#include <algorithm>
#include <limits>
#include <utility>

EarthOverlayCoordinator::EarthOverlayCoordinator(QObject *parent) : QObject(parent) {
    qRegisterMetaType<LarZoneMesh>("LarZoneMesh");
    m_thread.setObjectName(QStringLiteral("lar-viewport-preparation-thread"));
    m_worker = new ViewportPreparationWorker;
    m_worker->moveToThread(&m_thread);
    connect(m_worker, &ViewportPreparationWorker::meshReady, this,
            &EarthOverlayCoordinator::complete, Qt::QueuedConnection);
    m_thread.start();
}

EarthOverlayCoordinator::~EarthOverlayCoordinator() {
    if (!m_worker)
        return;
    m_worker->stop();
    m_thread.quit();
    m_thread.wait();
    delete m_worker;
    m_worker = nullptr;
}

void EarthOverlayCoordinator::setFallbackEnabled(bool enabled) {
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    ++m_revision;
    m_pending = false;
    m_prepared = false;
    m_mesh.clear();
    m_longitudeBounds.reset();
    m_dirty = enabled;
}

void EarthOverlayCoordinator::markDirty() noexcept {
    if (m_enabled)
        m_dirty = true;
}

void EarthOverlayCoordinator::request(const Target &target, const QBitArray &availableFields,
                                      const lar::map::MapCamera &camera, int width, int height) {
    if (!m_enabled || !m_worker)
        return;
    ViewportPreparationRequest request;
    request.revision = ++m_revision;
    request.target = target;
    request.availableFields = availableFields;
    request.camera = camera;
    request.width = width;
    request.height = height;
    m_pending = true;
    m_prepared = false;
    m_worker->submit(std::move(request));
}

void EarthOverlayCoordinator::complete(quint64 revision, const LarZoneMesh &mesh) {
    if (!m_enabled || revision != m_revision)
        return;
    m_pending = false;
    m_dirty = false;
    m_mesh = mesh;
    m_prepared = true;
    m_longitudeBounds.reset();
    if (!mesh.empty()) {
        double minimum = std::numeric_limits<double>::infinity();
        double maximum = -std::numeric_limits<double>::infinity();
        for (std::size_t offset = 0; offset + 2U < mesh.vertices.size(); offset += 3U) {
            const double longitude =
                static_cast<double>(mesh.vertices[offset]) +
                (mesh.coordinateSpace == LarZoneCoordinateSpace::GeographicDegrees
                     ? 0.0
                     : mesh.coordinateOrigin.x());
            minimum = std::min(minimum, longitude);
            maximum = std::max(maximum, longitude);
        }
        if (minimum <= maximum)
            m_longitudeBounds = {{minimum, maximum}};
    }
    emit meshAvailable(mesh.inputRejected);
}

bool EarthOverlayCoordinator::takePreparedMesh(LarZoneMesh *mesh) {
    if (!mesh || !m_prepared || !m_enabled)
        return false;
    *mesh = m_mesh;
    m_prepared = false;
    return true;
}
