#pragma once

/**
 * @file recording_transaction.h
 * @brief Mutable append-only recording transaction abstraction.
 */

#include "application/ports/session_snapshot.h"
#include "application/session_timestamp.h"

#include <QByteArray>
#include <QString>

#include <cstdint>

/**
 * Mutable transaction used while a recording is active.
 *
 * Destination persistence is deliberately excluded. createSnapshot() returns
 * an immutable view of the bytes committed so far; an ISessionPersistence
 * implementation decides how and where those bytes are saved.
 */
class IRecordingTransaction {
  public:
    virtual ~IRecordingTransaction() = default;

    virtual bool begin(const QByteArray &mappingJson, QString *error = nullptr) = 0;
    virtual bool append(SessionTimestamp timestamp, const QByteArray &packet,
                        QString *error = nullptr) = 0;
    virtual bool reset(QString *error = nullptr) = 0;
    virtual bool createSnapshot(SessionSnapshot *snapshot, QString *error = nullptr) = 0;
    /**
     * @brief Cancels and releases the active transaction.
     *
     * @details Discards transaction-owned resources and makes the transaction inactive.
     */
    virtual void cancel() noexcept = 0;
    /**
     * @brief Returns whether begin() succeeded and the transaction is not cancelled.
     *
     * @return True when the reported condition holds; false otherwise.
     */
    virtual bool isActive() const noexcept = 0;
    virtual quint64 recordCount() const noexcept = 0;
};
