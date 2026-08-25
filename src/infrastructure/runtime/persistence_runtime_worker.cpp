
#include "infrastructure/runtime/persistence_runtime_worker.h"

#include "infrastructure/session/qt_session_persistence.h"

PersistenceRuntimeWorker::PersistenceRuntimeWorker(std::unique_ptr<ISessionPersistence> persistence,
                                                   QObject *parent)
    : QObject(parent), m_persistence(persistence ? std::move(persistence)
                                                 : std::make_unique<QtSessionPersistence>()) {}

void PersistenceRuntimeWorker::save(quint64 requestId, bool finalSave,
                                    const SessionSnapshot &snapshot, const QString &targetPath) {
    QString error;
    bool saved;
    if (m_shuttingDown) {
        error = QStringLiteral("Persistence worker is shutting down");
        saved = false;
    } else {
        saved = m_persistence->save(snapshot, targetPath, &error);
    }

    emit saveFinished(requestId, finalSave, saved, targetPath, error);
}

void PersistenceRuntimeWorker::shutdown() {
    m_shuttingDown = true;
}
