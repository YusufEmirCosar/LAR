#pragma once

/**
 * @file runtime_messages.h
 * @brief Typed command results and events crossing the application runtime port.
 */

#include "application/ip_access_policy.h"
#include "application/session_timestamp.h"
#include "domain/decoded_state.h"

#include <QByteArray>
#include <QMetaType>
#include <QString>

#include <cstddef>
#include <optional>

/** Identifies the mutually exclusive producer of published application state. */
enum class RuntimeStateSource : quint8 {
    None = 0,
    Online,
    Playback,
};

/**
 * Correlates an accepted command with exactly one typed completion.
 *
 * Zero is reserved as the invalid value used when no request is associated
 * with a message.
 */
struct RuntimeRequestId final {
    quint64 value = 0;
    [[nodiscard]] bool isValid() const noexcept {
        return value != 0;
    }
    friend bool operator==(RuntimeRequestId left, RuntimeRequestId right) noexcept {
        return left.value == right.value;
    }
    friend bool operator!=(RuntimeRequestId left, RuntimeRequestId right) noexcept {
        return !(left == right);
    }
};

/**
 * Identifies one activation generation of an online or playback source.
 *
 * Epochs prevent queued publications from a stopped source from mutating the
 * state of a newer source activation.
 */
struct RuntimeSourceEpoch final {
    /** Producer kind; `None` makes the epoch invalid. */
    RuntimeStateSource source = RuntimeStateSource::None;
    /** Monotonic activation generation; zero makes the epoch invalid. */
    quint64 generation = 0;
    [[nodiscard]] bool isValid() const noexcept {
        return source != RuntimeStateSource::None && generation != 0;
    }
    friend bool operator==(const RuntimeSourceEpoch &left,
                           const RuntimeSourceEpoch &right) noexcept {
        return left.source == right.source && left.generation == right.generation;
    }
    friend bool operator!=(const RuntimeSourceEpoch &left,
                           const RuntimeSourceEpoch &right) noexcept {
        return !(left == right);
    }
};

inline std::size_t qHash(RuntimeRequestId id, std::size_t seed = 0) noexcept {
    return qHash(id.value, seed);
}

inline std::size_t qHash(const RuntimeSourceEpoch &epoch, std::size_t seed = 0) noexcept {
    return qHashMulti(seed, static_cast<quint8>(epoch.source), epoch.generation);
}

/**
 * Immediate structural acceptance or rejection of a runtime command.
 *
 * An accepted dispatch carries a valid request ID and receives one typed
 * completion, either during direct dispatch or later through a queued runtime.
 * A rejected dispatch carries the user-facing reason and receives no
 * completion.
 */
struct CommandDispatch final {
    RuntimeRequestId request;
    bool accepted = false;
    QString rejectionReason;
    /**
     * @brief Reports whether the runtime command was accepted.
     *
     * @details The conversion exposes the accepted flag for concise command-dispatch
     * checks.
     *
     * @return True when the command was accepted; false when it was rejected.
     */
    operator bool() const noexcept {
        return accepted;
    }
};

/** Identifies commands whose completion has no more specific result payload. */
enum class RuntimeCommandKind : quint8 {
    RecordingStart,
    RecordingPause,
    RecordingResume,
    RecordingDiscard,
    PlaybackPlay,
    PlaybackPause,
    PlaybackStop,
    PlaybackSeek,
    PlaybackRateChange,
    PlaybackRepeatChange,
    MetricsReset,
};

/** Exactly-once completion for a command without a dedicated result structure. */
struct RuntimeCommandResult final {
    RuntimeRequestId request;
    RuntimeCommandKind command = RuntimeCommandKind::MetricsReset;
    bool succeeded = false;
    QString error;
};

/** Completion payload for a mapping-load request. */
struct MappingLoadResult final {
    RuntimeRequestId request;
    bool loaded = false;
    QString path;
    int mappedFieldCount = 0;
    int minimumPacketSize = 0;
    QByteArray mappingJson;
    QString error;
};

/** Completion payload for activation of a new online source epoch. */
struct OnlineStartResult final {
    RuntimeRequestId request;
    RuntimeSourceEpoch epoch;
    bool started = false;
    QString error;
};

/** Completion payload for deactivation of an online source epoch. */
struct OnlineStopResult final {
    RuntimeRequestId request;
    RuntimeSourceEpoch epoch;
    bool stopped = false;
    QString error;
};

/** Current listening state published by the active network worker. */
struct OnlineStateEvent final {
    RuntimeSourceEpoch epoch;
    bool listening = false;
    quint16 port = 0;
};

/** Completion payload that retains the actually applied access policy. */
struct IpPolicyChangeResult final {
    RuntimeRequestId request;
    bool applied = false;
    IpAccessPolicy policy;
    QString error;
};

/** One atomic decoded frame tagged with its producing source epoch. */
struct StateEvent final {
    RuntimeSourceEpoch epoch;
    DecodedState state;
};

/** Packet or record throughput counters for one source epoch. */
struct MetricsEvent final {
    RuntimeSourceEpoch epoch;
    quint64 processedCount = 0;
    quint64 packetsPerSecond = 0;
};

/** Authoritative recording transaction state and committed record count. */
struct RecordingStateEvent final {
    bool hasSession = false;
    bool paused = false;
    quint64 recordCount = 0;
    SessionTimestamp duration;
};

/** Completion payload for a snapshot or final recording save. */
struct RecordingSaveResult final {
    RuntimeRequestId request;
    /** `true` for stop-and-finalize, `false` for a non-destructive snapshot. */
    bool finalSave = false;
    bool saved = false;
    QString path;
    QString error;
};

/** Completion payload for clearing the active recording transaction. */
struct RecordingResetResult final {
    RuntimeRequestId request;
    bool reset = false;
    QString error;
};

/** Completion payload for loading and indexing a playback session. */
struct SessionLoadResult final {
    RuntimeRequestId request;
    RuntimeSourceEpoch epoch;
    bool loaded = false;
    QString path;
    qint64 recordCount = 0;
    SessionTimestamp duration;
    QString error;
};

/** Completion payload for closing a playback session and its epoch. */
struct SessionCloseResult final {
    RuntimeRequestId request;
    RuntimeSourceEpoch epoch;
    bool closed = false;
    QString error;
};

/** Current exact playback position for the active playback epoch. */
struct PlaybackPositionEvent final {
    RuntimeSourceEpoch epoch;
    SessionTimestamp position;
};

/** Playing/paused state for the active playback epoch. */
struct PlaybackStateEvent final {
    RuntimeSourceEpoch epoch;
    bool playing = false;
};

/** Announces natural non-repeating completion for the active playback epoch. */
struct PlaybackFinishedEvent final {
    RuntimeSourceEpoch epoch;
};

/** Stable failure category used to distinguish operational and lifecycle errors. */
enum class RuntimeFailureCode : quint8 {
    Operational,
    Construction,
    Shutdown,
};

/**
 * Diagnostic associated with an optional request and source epoch.
 *
 * The request is invalid for unsolicited failures. The optional epoch lets a
 * consumer discard a queued failure from a source activation it no longer
 * owns.
 */
struct RuntimeFailure final {
    RuntimeRequestId request;
    std::optional<RuntimeSourceEpoch> epoch;
    RuntimeFailureCode code = RuntimeFailureCode::Operational;
    QString message;
};

/**
 * @brief Registers the complete queued-delivery protocol exactly once per process.
 */
void registerRuntimeMessageTypes();

Q_DECLARE_METATYPE(RuntimeStateSource)
Q_DECLARE_METATYPE(RuntimeRequestId)
Q_DECLARE_METATYPE(RuntimeSourceEpoch)
Q_DECLARE_METATYPE(CommandDispatch)
Q_DECLARE_METATYPE(RuntimeCommandKind)
Q_DECLARE_METATYPE(RuntimeCommandResult)
Q_DECLARE_METATYPE(MappingLoadResult)
Q_DECLARE_METATYPE(OnlineStartResult)
Q_DECLARE_METATYPE(OnlineStopResult)
Q_DECLARE_METATYPE(OnlineStateEvent)
Q_DECLARE_METATYPE(IpPolicyChangeResult)
Q_DECLARE_METATYPE(StateEvent)
Q_DECLARE_METATYPE(MetricsEvent)
Q_DECLARE_METATYPE(RecordingStateEvent)
Q_DECLARE_METATYPE(RecordingSaveResult)
Q_DECLARE_METATYPE(RecordingResetResult)
Q_DECLARE_METATYPE(SessionLoadResult)
Q_DECLARE_METATYPE(SessionCloseResult)
Q_DECLARE_METATYPE(PlaybackPositionEvent)
Q_DECLARE_METATYPE(PlaybackStateEvent)
Q_DECLARE_METATYPE(PlaybackFinishedEvent)
Q_DECLARE_METATYPE(RuntimeFailureCode)
Q_DECLARE_METATYPE(RuntimeFailure)
