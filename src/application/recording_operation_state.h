#pragma once

/**
 * @file recording_operation_state.h
 * @brief Mutually exclusive persistence-operation state for a recording session.
 */

#include <QMetaType>

/** Authoritative application state for mutually exclusive recording operations. */
enum class RecordingOperationState : quint8 {
    Idle,
    SavingSnapshot,
    Finalizing,
    Resetting,
    FailedRetained,
};

inline bool isRecordingOperationPending(RecordingOperationState state) noexcept {
    return state == RecordingOperationState::SavingSnapshot ||
           state == RecordingOperationState::Finalizing ||
           state == RecordingOperationState::Resetting;
}

Q_DECLARE_METATYPE(RecordingOperationState)
