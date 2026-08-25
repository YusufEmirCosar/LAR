#pragma once

/**
 * @file application_runtime.h
 * @brief Ticketed command and typed event boundary for application execution.
 */

#include "application/ports/runtime_messages.h"

#include <QObject>

#include <atomic>

/**
 * Every structurally valid command returns an accepted dispatch with a unique
 * non-zero request ID. Operational success or failure is reported through the
 * matching typed result/failure signal. Invalid arguments and post-shutdown
 * commands are rejected immediately and produce no operational completion.
 */
/** Segregated command port for mapping, access policy, and online capture. */
class IMappingCaptureRuntime {
  public:
    virtual ~IMappingCaptureRuntime() = default;
    virtual CommandDispatch loadMapping(const QString &path) = 0;
    virtual CommandDispatch startOnline(quint16 port) = 0;
    virtual CommandDispatch setIpAccessPolicy(const IpAccessPolicy &policy) = 0;
    virtual CommandDispatch stopOnline() = 0;
};

/** Segregated command port for the active recording transaction. */
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

/** Segregated command port for session loading and playback control. */
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

/** Segregated command port for clearing source throughput counters. */
class IMetricsRuntime {
  public:
    virtual ~IMetricsRuntime() = default;
    virtual CommandDispatch resetMetrics() = 0;
};

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
 * protocol shared by direct and threaded runtime implementations.
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
