
#include "viewer/viewport/viewport_preparation_worker.h"

#include <utility>

ViewportPreparationWorker::ViewportPreparationWorker(QObject *parent) : QObject(parent) {
    connect(this, &ViewportPreparationWorker::wakeRequested, this,
            &ViewportPreparationWorker::processLatest, Qt::QueuedConnection);
}

void ViewportPreparationWorker::submit(ViewportPreparationRequest request) {
    if (m_stopped.load(std::memory_order_acquire))
        return;
    m_latestRevision.store(request.revision, std::memory_order_release);
    {
        const QMutexLocker locker(&m_mutex);
        m_pending = std::move(request);
        m_hasPending = true;
    }
    if (!m_wakePosted.exchange(true, std::memory_order_acq_rel)) {
        emit wakeRequested();
    }
}

void ViewportPreparationWorker::stop() noexcept {
    m_stopped.store(true, std::memory_order_release);
    m_latestRevision.fetch_add(1, std::memory_order_acq_rel);
    const QMutexLocker locker(&m_mutex);
    m_hasPending = false;
}

void ViewportPreparationWorker::processLatest() {
    m_wakePosted.store(false, std::memory_order_release);
    while (!m_stopped.load(std::memory_order_acquire)) {
        ViewportPreparationRequest request;
        {
            const QMutexLocker locker(&m_mutex);
            if (!m_hasPending)
                break;
            request = std::move(m_pending);
            m_hasPending = false;
        }

        // A newer request can arrive while the bounded builder is running.
        // Keep the work off the GUI thread, but never publish a stale mesh.
        const LarZoneMesh mesh = m_builder.build(request.target, request.availableFields,
                                                 request.camera, request.width, request.height);
        if (m_stopped.load(std::memory_order_acquire))
            break;
        if (m_latestRevision.load(std::memory_order_acquire) == request.revision) {
            emit meshReady(request.revision, mesh);
        }
    }
}
