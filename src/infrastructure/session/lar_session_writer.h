#pragma once

/**
 * @file lar_session_writer.h
 * @brief Temporary-file implementation of an active recording transaction.
 */

#include "application/ports/recording_transaction.h"
#include "domain/packet_mapping.h"

#include <QTemporaryFile>

#include <memory>

/**
 * @brief Appends timestamped packets and creates immutable file snapshots.
 */
class LarSessionWriter final : public IRecordingTransaction {
  public:
    LarSessionWriter() = default;
    ~LarSessionWriter() override;

    bool begin(const QByteArray &mappingJson, QString *error = nullptr) override;
    bool append(SessionTimestamp timestamp, const QByteArray &packet,
                QString *error = nullptr) override;
    bool reset(QString *error = nullptr) override;
    bool createSnapshot(SessionSnapshot *snapshot, QString *error = nullptr) override;
    void cancel() noexcept override;
    bool isActive() const noexcept override;
    quint64 recordCount() const noexcept override;

  private:
    bool openSession(const QByteArray &mappingJson, QString *error);

    std::shared_ptr<QTemporaryFile> m_file;
    PacketMapping m_mapping;
    QByteArray m_mappingJson;
    quint64 m_lastTimestampMs = 0;
    quint64 m_recordCount = 0;
};
