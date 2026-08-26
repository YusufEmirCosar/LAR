#pragma once

/**
 * @file session_reader.h
 * @brief Random-access session playback reader contract.
 */

#include "application/session_timestamp.h"
#include "domain/decoded_state.h"

#include <QBitArray>
#include <QByteArray>
#include <QString>
#include <QVector>

/** @brief One decoded session record and its relative timestamp. */
struct SessionStateItem {
    DecodedState state;
    SessionTimestamp timestamp;
};

/** @brief Loads, indexes, and decodes records without playback policy. */
class ISessionReader {
  public:
    virtual ~ISessionReader() = default;

    virtual bool loadFile(const QString &path, QString *error = nullptr) = 0;
    virtual bool loadData(const QByteArray &data, QString *error = nullptr) = 0;
    /**
     * @brief Releases the current source and returns the reader to its empty state.
     */
    virtual void close() noexcept = 0;
    /** True only while a fully validated session remains loaded. */
    virtual bool isValid() const noexcept = 0;
    /** @brief Returns the number of records validated in the loaded session. */
    virtual qint64 recordCount() const noexcept = 0;
    virtual SessionTimestamp duration() const noexcept = 0;
    virtual bool timestampAt(qint64 index, SessionTimestamp *timestamp,
                             QString *error = nullptr) const = 0;
    virtual bool recordAt(qint64 index, SessionStateItem *item, QString *error = nullptr) const = 0;
};
