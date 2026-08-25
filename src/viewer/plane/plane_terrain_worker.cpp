#include "viewer/plane/plane_terrain_worker.h"

#include <QMutexLocker>

#include <utility>

PlaneTerrainWorker::PlaneTerrainWorker(QString dtedRootDirectory, QString waterMaskPackPath,
                                       QObject *parent)
    : QObject(parent), m_builder(std::move(dtedRootDirectory), std::move(waterMaskPackPath)) {
    connect(this, &PlaneTerrainWorker::wakeRequested, this, &PlaneTerrainWorker::processLatest,
            Qt::QueuedConnection);
}

void PlaneTerrainWorker::submit(PlaneTerrainBuildRequest request) {
    if (m_stopped.load(std::memory_order_acquire)) {
        return;
    }
    m_latestRevision.store(request.revision, std::memory_order_release);
    {
        const QMutexLocker locker(&m_mutex);
        m_pending = request;
        m_hasPending = true;
    }
    if (!m_wakePosted.exchange(true, std::memory_order_acq_rel)) {
        emit wakeRequested();
    }
}

void PlaneTerrainWorker::stop() noexcept {
    m_stopped.store(true, std::memory_order_release);
    m_latestRevision.fetch_add(1, std::memory_order_acq_rel);
    const QMutexLocker locker(&m_mutex);
    m_hasPending = false;
}

void PlaneTerrainWorker::processLatest() {
    m_wakePosted.store(false, std::memory_order_release);
    while (!m_stopped.load(std::memory_order_acquire)) {
        PlaneTerrainBuildRequest request;
        {
            const QMutexLocker locker(&m_mutex);
            if (!m_hasPending) {
                break;
            }
            request = m_pending;
            m_hasPending = false;
        }
        QString message;
        PlaneTerrainPatchPtr patch = m_builder.build(request, &message);
        if (m_stopped.load(std::memory_order_acquire)) {
            break;
        }
        if (m_latestRevision.load(std::memory_order_acquire) == request.revision) {
            emit patchReady(request.revision, std::move(patch), message);
        }
    }
}
