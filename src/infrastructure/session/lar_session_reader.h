#pragma once

/**
 * @file lar_session_reader.h
 * @brief Bounded parser with sparse, paged random access for .lar session files.
 */

#include "application/ports/session_reader.h"
#include "domain/packet_mapping.h"

#include <QFile>

#include <memory>

/**
 * @brief Validates a session once, checkpoints it sparsely, and decodes on demand.
 *
 * The reader deliberately has no record-count policy limit. A timestamp and
 * header offset are retained for every 4,096 records, and only one page of
 * record locations is cached. The two-level lookup therefore avoids both a
 * full per-record memory index and cross-page seek amplification. Like QFile,
 * random-access methods are confined to the reader's owning thread.
 */
class LarSessionReader final : public ISessionReader {
  public:
    LarSessionReader() = default;
    ~LarSessionReader() override;

    bool loadFile(const QString &path, QString *error = nullptr) override;
    bool loadData(const QByteArray &data, QString *error = nullptr) override;
    /** Releases the source, sparse index, decoded mapping, and cached record page. */
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
    mutable QVector<RecordIndex> m_page;
    mutable qint64 m_pageFirstRecord = -1;
    PacketMapping m_mapping;
    QByteArray m_mappingJson;
    QByteArray m_sourceData;
    mutable std::unique_ptr<QFile> m_sourceFile;
    qint64 m_sourceSize = 0;
    qint64 m_recordCount = 0;
    SessionTimestamp m_duration;
    bool m_isValid = false;
};
