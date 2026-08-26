#pragma once

/**
 * @file application_runtime.h
 * @brief Ticketed command and typed event boundary for application execution.
 */

#include "application/ports/runtime_messages.h"

#include <QObject>

#include <atomic>

/**
 * @defgroup runtime_command_ports Runtime command ports
 * @brief Shared command-dispatch contract for every runtime command port.
 *
 * A structurally valid command returns an accepted dispatch with a unique,
 * non-zero request ID. The runtime reports exactly one operational completion
 * bearing that ID through the signal assigned to the command. Delivery may be
 * synchronous in the direct runtime or queued in the threaded runtime.
 * Invalid arguments and commands issued after shutdown are rejected
 * synchronously: their dispatch has no valid request ID and no completion is
 * emitted.
 *
 * Acceptance only means that ownership of the request transferred to the
 * runtime. Callers must not treat it as operational success.
 * @{
 */
/**
 * @brief Command port for mapping, access policy, and online capture.
 *
 * `loadMapping()` completes through `mappingLoadFinished()`, `startOnline()`
 * and `stopOnline()` through their corresponding online result signals, and
 * `setIpAccessPolicy()` through `ipAccessPolicyChangeFinished()`.
 */
class IMappingCaptureRuntime {
  public:
    virtual ~IMappingCaptureRuntime() = default;
    virtual CommandDispatch loadMapping(const QString &path) = 0;
    virtual CommandDispatch startOnline(quint16 port) = 0;
    virtual CommandDispatch setIpAccessPolicy(const IpAccessPolicy &policy) = 0;
    virtual CommandDispatch stopOnline() = 0;
};

/**
 * @brief Command port for the active recording transaction.
 *
 * Start, pause, resume, and discard complete through `commandFinished()`.
 * Snapshot and final-save requests complete through `recordingSaveFinished()`;
 * its `finalSave` flag distinguishes the two. Reset completes through
 * `recordingResetFinished()`.
 */
class IRecordingRuntime {
  public:
    virtual ~IRecordingRuntime() = default;
    virtual CommandDispatch startRecording(const QByteArray &mappingJson) = 0;
    virtual CommandDispatch pauseRecording() = 0;
    virtual CommandDispatch resumeRecording() = 0;
    virtual CommandDispatch stopRecording(const QString &targetPath) = 0;
    virtual CommandDispatch resetRecording() = 0;
    virtual CommandDispatch snapshotRecording(const QString &targetPath) = 0;
    virtual CommandDispatch discardRecording() = 0;
};

/**
 * @brief Command port for session loading and playback control.
 *
 * Loading and closing complete through `sessionLoadFinished()` and
 * `sessionClosed()`. Playback controls complete through `commandFinished()`;
 * position and playing-state events are publications, not command
 * acknowledgements.
 */
class IPlaybackRuntime {
  public:
    virtual ~IPlaybackRuntime() = default;
    virtual CommandDispatch loadSession(const QString &path) = 0;
    virtual CommandDispatch closeSession() = 0;
    virtual CommandDispatch play() = 0;
    virtual CommandDispatch pause() = 0;
    virtual CommandDispatch stop() = 0;
    virtual CommandDispatch seek(SessionTimestamp position) = 0;
    virtual CommandDispatch setPlaybackRate(double rate) = 0;
    virtual CommandDispatch setPlaybackRepeat(bool enabled) = 0;
};

/**
 * @brief Command port for clearing source throughput counters.
 *
 * `resetMetrics()` completes through `commandFinished()`.
 */
class IMetricsRuntime {
  public:
    virtual ~IMetricsRuntime() = default;
    virtual CommandDispatch resetMetrics() = 0;
};

/** @} */

/** Minimal lifecycle port for deterministic worker shutdown. */
class IApplicationLifetime {
  public:
    virtual ~IApplicationLifetime() = default;
    /**
     * @brief Stops accepting commands, drains workers, and joins owned threads.
     */
    virtual void shutdown() = 0;
};

/**
 * Composite runtime boundary used by ApplicationFacade.
 *
 * The smaller base interfaces preserve interface segregation for focused
 * consumers and test doubles; this QObject adds the typed asynchronous event
 * protocol shared by direct and threaded runtime implementations. Result
 * signals are request-correlated completions. State, metrics, and playback
 * signals are epoch-correlated publications and may occur repeatedly.
 */
class IApplicationRuntime : public QObject,
                            public IMappingCaptureRuntime,
                            public IRecordingRuntime,
                            public IPlaybackRuntime,
                            public IMetricsRuntime,
                            public IApplicationLifetime {
    Q_OBJECT

  public:
    explicit IApplicationRuntime(QObject *parent = nullptr) : QObject(parent) {}
    ~IApplicationRuntime() override = default;

  signals:
    /** Completes an accepted command that has no command-specific result type. */
    void commandFinished(const RuntimeCommandResult &result);
    void mappingLoadFinished(const MappingLoadResult &result);
    void onlineStateChanged(const OnlineStateEvent &event);
    void onlineStartFinished(const OnlineStartResult &result);
    void onlineStopFinished(const OnlineStopResult &result);
    void ipAccessPolicyChangeFinished(const IpPolicyChangeResult &result);
    void stateReady(const StateEvent &event);
    void metricsChanged(const MetricsEvent &event);
    void recordingStateChanged(const RecordingStateEvent &event);
    void recordingSaveFinished(const RecordingSaveResult &result);
    void recordingResetFinished(const RecordingResetResult &result);
    void sessionLoadFinished(const SessionLoadResult &result);
    void sessionClosed(const SessionCloseResult &result);
    void playbackPositionChanged(const PlaybackPositionEvent &event);
    void playbackPlayingChanged(const PlaybackStateEvent &event);
    void playbackFinished(const PlaybackFinishedEvent &event);
    void runtimeError(const RuntimeFailure &failure);

  protected:
    [[nodiscard]] CommandDispatch acceptCommand() {
        return {RuntimeRequestId{m_nextRequest.fetch_add(1, std::memory_order_relaxed)}, true, {}};
    }

    [[nodiscard]] static CommandDispatch rejectCommand(const QString &reason) {
        return {{}, false, reason};
    }

  private:
    std::atomic<quint64> m_nextRequest{1};
};
