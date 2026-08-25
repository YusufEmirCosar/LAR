#pragma once

/**
 * @file session_snapshot.h
 * @brief Type-erased immutable snapshot shared with persistence workers.
 */

#include <QMetaType>
#include <QString>

#include <memory>

class QIODevice;

/**
 * Opaque immutable recording snapshot.
 *
 * The application layer can pass a snapshot to persistence without knowing
 * whether its bytes live in memory, a temporary file, or another storage
 * mechanism. Every implementation exposes the same write contract, so atomic
 * persistence does not need to downcast to its storage representation.
 */
class ISessionSnapshot {
  public:
    virtual ~ISessionSnapshot() = default;

    virtual bool writeTo(QIODevice &destination, QString *error = nullptr) const = 0;
};

using SessionSnapshot = std::shared_ptr<const ISessionSnapshot>;

Q_DECLARE_METATYPE(SessionSnapshot)
