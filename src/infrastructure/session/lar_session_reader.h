#pragma once

/**
 * @file lar_session_reader.h
 * @brief Bounded parser with resident and sparse random access for .lar files.
 */

#include "application/ports/session_reader.h"
#include "domain/packet_mapping.h"

#include <QFile>

#include <memory>

/**
 * @brief Validates a session once, indexes it, and decodes selected records on demand.
 *
 * The reader has no record-count policy limit. Normal recordings retain an
 * immutable source snapshot plus a complete per-record location index, making
 * high-rate random access independent of file-system seek performance.
 * Resource limits are adaptive: oversized sources remain file-backed and
 * indexes exceeding the resident-index budget fall back to timestamped
 * checkpoints plus one page covering at most 4,096 records. Random-access
 * methods remain confined to the reader's owning thread.
 */
class LarSessionReader final : public ISessionReader {
  public:
    LarSessionReader() = default;
    ~LarSessionReader() override;

    bool loadFile(const QString &path, QString *error = nullptr) override;
    bool loadData(const QByteArray &data, QString *error = nullptr) override;
    /** Releases every source, index, decoded mapping, and cached record page. */
    void close() noexcept override;
    bool isValid() const noexcept override {
        return m_isValid;
    }
    qint64 recordCount() const noexcept override {
        return m_recordCount;
    }
    SessionTimestamp duration() const noexcept override {
        return m_duration;
    }
    bool findRecordAtOrBefore(SessionTimestamp position, qint64 *index,
                              QString *error = nullptr) const override;
    bool timestampAt(qint64 index, SessionTimestamp *timestamp,
                     QString *error = nullptr) const override;
    bool recordAt(qint64 index, SessionStateItem *item, QString *error = nullptr) const override;
    const PacketMapping &mapping() const noexcept {
        return m_mapping;
    }

  private:
    struct RecordIndex {
        SessionTimestamp timestamp;
        qint64 payloadOffset = 0;
        quint32 payloadSize = 0;
    };

    struct Checkpoint {
        qint64 recordIndex = 0;
        qint64 headerOffset = 0;
        SessionTimestamp timestamp;
    };

    static constexpr qint64 RecordsPerPage = 4096;

    bool parseStream(QIODevice *device, QString *error);

    /** Populates the bounded location cache containing @p index. */
    bool ensurePage(qint64 index, QString *error) const;

    /** Returns the cached location for @p index after range and source validation. */
    const RecordIndex *locationAt(qint64 index, QString *error) const;

    QVector<Checkpoint> m_checkpoints;
    QVector<RecordIndex> m_residentIndex;
    mutable QVector<RecordIndex> m_page;
    mutable qint64 m_pageFirstRecord = -1;
    PacketMapping m_mapping;
    QByteArray m_mappingJson;
    QByteArray m_sourceData;
    mutable std::unique_ptr<QFile> m_sourceFile;
    qint64 m_sourceSize = 0;
    qint64 m_recordCount = 0;
    SessionTimestamp m_duration;
    bool m_residentIndexAvailable = false;
    bool m_isValid = false;
};
