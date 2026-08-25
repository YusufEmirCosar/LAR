#pragma once

/**
 * @file viewport_preparation_worker.h
 * @brief Latest-only CPU preparation worker for dynamic LAR geometry.
 */

#include "domain/state.h"
#include "viewer/map/map_camera.h"
#include "viewer/viewport/lar_zone_mesh.h"
#include "viewer/viewport/lar_zone_mesh_builder.h"

#include <QBitArray>
#include <QMutex>
#include <QObject>

#include <atomic>

/** @brief Immutable request consumed by the dedicated viewport thread. */
struct ViewportPreparationRequest final {
    quint64 revision = 0;
    Target target{};
    QBitArray availableFields;
    lar::map::MapCamera camera;
    int width = 0;
    int height = 0;
};

Q_DECLARE_METATYPE(ViewportPreparationRequest)

/**
 * @brief Builds CPU fallback meshes away from QWidget and GUI event handling.
 *
 * submit() is thread-safe and coalesces pending requests.  At most one build
 * runs at a time, and a completed result is discarded when a newer revision
 * has already been submitted.
 */
class ViewportPreparationWorker final : public QObject {
    Q_OBJECT

  public:
    explicit ViewportPreparationWorker(QObject *parent = nullptr);

    void submit(ViewportPreparationRequest request);
    void stop() noexcept;

  signals:
    /**
     * @brief Posts one coalesced, argument-free worker wake-up.
     */
    void wakeRequested();

    void meshReady(quint64 revision, const LarZoneMesh &mesh);

  private slots:
    void processLatest();

  private:
    QMutex m_mutex;
    ViewportPreparationRequest m_pending;
    bool m_hasPending = false;
    std::atomic<quint64> m_latestRevision{0};
    std::atomic_bool m_stopped{false};
    std::atomic_bool m_wakePosted{false};
    LarZoneMeshBuilder m_builder;
};
