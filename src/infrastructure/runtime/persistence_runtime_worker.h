#pragma once

/**
 * @file persistence_runtime_worker.h
 * @brief File-thread worker that saves immutable session snapshots.
 */

#include "application/ports/session_persistence.h"

#include <QObject>
#include <QString>

#include <memory>

/** @brief Serializes persistence requests away from capture and the UI. */
class PersistenceRuntimeWorker final : public QObject {
    Q_OBJECT

  public:
    explicit PersistenceRuntimeWorker(std::unique_ptr<ISessionPersistence> persistence = {},
                                      QObject *parent = nullptr);

  public slots:
    void save(quint64 requestId, bool finalSave, const SessionSnapshot &snapshot,
              const QString &targetPath);
    /**
     * @brief Shuts down the persistence worker.
     */
    void shutdown();

  signals:
    void saveFinished(quint64 requestId, bool finalSave, bool saved, const QString &targetPath,
                      const QString &error);

  private:
    std::unique_ptr<ISessionPersistence> m_persistence;
    bool m_shuttingDown = false;
};
