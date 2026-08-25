#pragma once

/**
 * @file session_persistence.h
 * @brief Atomic destination persistence contract for session snapshots.
 */

#include "application/ports/session_snapshot.h"

#include <QString>

/**
 * Persists an immutable session snapshot to a destination.
 *
 * Implementations must either atomically replace the destination with the
 * complete snapshot and return true, or leave the previous destination intact
 * and return false.
 */
class ISessionPersistence {
  public:
    virtual ~ISessionPersistence() = default;

    virtual bool save(const SessionSnapshot &snapshot, const QString &targetPath,
                      QString *error = nullptr) = 0;
};
