#include "viewer/plane/plane_terrain_worker.h"

#include <QMutexLocker>

#include <utility>

PlaneTerrainWorker::PlaneTerrainWorker(
    DtedDataset dataset, std::shared_ptr<const lar::map::IMapAssetSource> mapAssetSource,
    QObject *parent)
    : QObject(parent), m_dataset(std::move(dataset)), m_mapAssetSource(std::move(mapAssetSource)) {
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
        if (!m_builderInitializationAttempted) {
            m_builderInitializationAttempted = true;
            if (m_mapAssetSource == nullptr) {
                m_builderError = QStringLiteral("The packaged vector land map is unavailable.");
            } else {
                try {
                    const lar::map::MapAssetReadResult mapResult = m_mapAssetSource->load();
                    lar::map::MapLandIndex landIndex(mapResult.mesh);
                    if (!mapResult.succeeded()) {
                        m_builderError =
                            mapResult.message.isEmpty()
                                ? QStringLiteral(
                                      "The packaged vector land map could not be loaded.")
                                : mapResult.message;
                    } else if (!landIndex.isValid()) {
                        m_builderError =
                            QStringLiteral("The packaged vector land index is unavailable.");
                    } else {
                        m_builder = std::make_unique<PlaneTerrainPatchBuilder>(
                            m_dataset, std::move(landIndex));
                    }
                } catch (...) {
                    m_builderError =
                        QStringLiteral("The packaged vector land map failed unexpectedly.");
                }
            }
        }
        PlaneTerrainPatchPtr patch;
        if (m_builder != nullptr) {
            patch = m_builder->build(request, &message, [this, &request] {
                return m_stopped.load(std::memory_order_acquire) ||
                       m_latestRevision.load(std::memory_order_acquire) != request.revision;
            });
        } else {
            message = m_builderError;
        }
        if (m_stopped.load(std::memory_order_acquire)) {
            break;
        }
        if (m_latestRevision.load(std::memory_order_acquire) == request.revision) {
            emit patchReady(request.revision, std::move(patch), message);
        }
    }
}
