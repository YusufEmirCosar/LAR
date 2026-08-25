#pragma once

/**
 * @file qt_session_persistence.h
 * @brief Atomic Qt filesystem persistence for polymorphic session snapshots.
 */

#include "application/ports/session_persistence.h"

/** @brief Commits a snapshot with QSaveFile semantics. */
class QtSessionPersistence final : public ISessionPersistence {
  public:
    bool save(const SessionSnapshot &snapshot, const QString &targetPath,
              QString *error = nullptr) override;
};
