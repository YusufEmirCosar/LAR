#pragma once

/**
 * @file file_session_snapshot.h
 * @brief Immutable file-backed recording snapshot implementation.
 */

#include "application/ports/session_snapshot.h"

#include <QString>

#include <utility>

/**
 * @brief Describes a stable byte prefix of a file and retains its lifetime.
 */
class FileSessionSnapshot final : public ISessionSnapshot {
  public:
    FileSessionSnapshot(QString sourcePath, qint64 byteCount,
                        std::shared_ptr<const void> sourceLifetime = {})
        : m_sourcePath(std::move(sourcePath)), m_byteCount(byteCount),
          m_sourceLifetime(std::move(sourceLifetime)) {}

    const QString &sourcePath() const noexcept {
        return m_sourcePath;
    }
    qint64 byteCount() const noexcept {
        return m_byteCount;
    }

    bool writeTo(QIODevice &destination, QString *error = nullptr) const override;

  private:
    QString m_sourcePath;
    qint64 m_byteCount = 0;
    // Keeps auto-removing temporary storage alive until persistence releases
    // the immutable snapshot, independently of the recording transaction.
    std::shared_ptr<const void> m_sourceLifetime;
};
