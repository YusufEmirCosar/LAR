#pragma once

/**
 * @file plane_terrain_worker.h
 * @brief Latest-only dedicated CPU worker for DTED terrain patch preparation.
 */

#include "viewer/plane/plane_terrain_patch_builder.h"

#include <QMutex>
#include <QObject>

#include <atomic>

/** @brief Coalesces aircraft movement so only the newest bounded terrain patch is published. */
class PlaneTerrainWorker final : public QObject {
    Q_OBJECT

  public:
    PlaneTerrainWorker(QString dtedRootDirectory, QString waterMaskPackPath,
                       QObject *parent = nullptr);

    void submit(PlaneTerrainBuildRequest request);
    void stop() noexcept;

  signals:
    void wakeRequested();
    void patchReady(quint64 revision, PlaneTerrainPatchPtr patch, const QString &message);

  private slots:
    void processLatest();

  private:
    QMutex m_mutex;
    PlaneTerrainBuildRequest m_pending;
    bool m_hasPending = false;
    std::atomic<quint64> m_latestRevision{0};
    std::atomic_bool m_stopped{false};
    std::atomic_bool m_wakePosted{false};
    PlaneTerrainPatchBuilder m_builder;
};
