#pragma once

/**
 * @file earth_overlay_coordinator.h
 * @brief Latest-only worker coordination for CPU fallback zone meshes.
 */

#include "viewer/viewport/viewport_preparation_worker.h"

#include <QObject>
#include <QThread>

#include <optional>
#include <utility>

/** Owns latest-only CPU overlay preparation and its worker-thread lifetime. */
class EarthOverlayCoordinator final : public QObject {
    Q_OBJECT

  public:
    explicit EarthOverlayCoordinator(QObject *parent = nullptr);
    ~EarthOverlayCoordinator() override;

    void setFallbackEnabled(bool enabled);
    bool fallbackEnabled() const noexcept {
        return m_enabled;
    }
    void markDirty() noexcept;
    void request(const Target &target, const QBitArray &availableFields,
                 const lar::map::MapCamera &camera, int width, int height);
    bool takePreparedMesh(LarZoneMesh *mesh);
    bool dirty() const noexcept {
        return m_dirty;
    }
    bool pending() const noexcept {
        return m_pending;
    }
    std::optional<std::pair<double, double>> longitudeBounds() const noexcept {
        return m_longitudeBounds;
    }

  signals:
    void meshAvailable(bool inputRejected);

  private slots:
    void complete(quint64 revision, const LarZoneMesh &mesh);

  private:
    QThread m_thread;
    ViewportPreparationWorker *m_worker = nullptr;
    quint64 m_revision = 0;
    bool m_enabled = true;
    bool m_dirty = true;
    bool m_pending = false;
    bool m_prepared = false;
    LarZoneMesh m_mesh;
    std::optional<std::pair<double, double>> m_longitudeBounds;
};
