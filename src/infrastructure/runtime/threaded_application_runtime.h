#pragma once

/**
 * @file threaded_application_runtime.h
 * @brief Production runtime that composes and coordinates worker threads.
 */

#include "application/ports/application_runtime.h"

#include <QThread>

#include <functional>

class NetworkRuntimeWorker;
class PlaybackRuntimeWorker;
class PersistenceRuntimeWorker;
class RecordingRuntimeWorker;

/** @brief Optional factories used to inject deterministic workers in tests. */
struct RuntimeWorkerFactories final {
    std::function<NetworkRuntimeWorker *()> createNetworkWorker;
    std::function<RecordingRuntimeWorker *()> createRecordingWorker;
    std::function<PlaybackRuntimeWorker *()> createPlaybackWorker;
    std::function<PersistenceRuntimeWorker *()> createPersistenceWorker;
};

/**
 * Production runtime with four total execution threads:
 * the caller/UI thread plus network, session, and persistence workers.
 *
 * All public commands, shutdown(), and destruction are confined to the runtime's
 * QObject affinity thread. shutdown() synchronously drains each worker, moves the
 * complete QObject tree back to that thread, joins the QThreads, and only then
 * destroys workers. No worker lifetime depends on deferred deletion after exit.
 */
class ThreadedApplicationRuntime final : public IApplicationRuntime {
    Q_OBJECT

  public:
    explicit ThreadedApplicationRuntime(QObject *parent = nullptr);
    explicit ThreadedApplicationRuntime(RuntimeWorkerFactories factories,
                                        QObject *parent = nullptr);
    ~ThreadedApplicationRuntime() override;

    int runningWorkerThreadCount() const noexcept;

    CommandDispatch loadMapping(const QString &path) override;
    CommandDispatch startOnline(quint16 port) override;
    CommandDispatch setIpAccessPolicy(const IpAccessPolicy &policy) override;
    CommandDispatch stopOnline() override;

    CommandDispatch startRecording(const QByteArray &mappingJson) override;
    CommandDispatch pauseRecording() override;
    CommandDispatch resumeRecording() override;
    CommandDispatch stopRecording(const QString &targetPath) override;
    CommandDispatch resetRecording() override;
    CommandDispatch snapshotRecording(const QString &targetPath) override;
    CommandDispatch discardRecording() override;

    CommandDispatch loadSession(const QString &path) override;
    CommandDispatch closeSession() override;
    CommandDispatch play() override;
    CommandDispatch pause() override;
    CommandDispatch stop() override;
    CommandDispatch seek(SessionTimestamp position) override;
    CommandDispatch setPlaybackRate(double rate) override;
    CommandDispatch setPlaybackRepeat(bool enabled) override;

    CommandDispatch resetMetrics() override;
    /** @brief Synchronously drains, joins, and destroys every owned worker. */
    void shutdown() override;

  signals:
    void mappingLoadRequested(const QString &path, RuntimeRequestId request);
    void onlineStartRequested(quint16 port, quint64 generation, RuntimeRequestId request);
    void ipAccessPolicyChangeRequested(const IpAccessPolicy &policy, RuntimeRequestId request);
    void onlineStopRequested(quint64 generation, RuntimeRequestId request);
    void networkMetricsResetRequested(RuntimeRequestId request);

    void recordingStartRequested(const QByteArray &mappingJson, RuntimeRequestId request);
    void recordingPauseRequested(RuntimeRequestId request);
    void recordingResumeRequested(RuntimeRequestId request);
    void recordingStopRequested(const QString &targetPath, RuntimeRequestId request);
    void recordingResetRequested(RuntimeRequestId request);
    void recordingSnapshotRequested(const QString &targetPath, RuntimeRequestId request);
    void recordingDiscardRequested(RuntimeRequestId request);

    void sessionLoadRequested(const QString &path, quint64 generation, RuntimeRequestId request);
    void sessionCloseRequested(quint64 generation, RuntimeRequestId request);
    void playbackStartRequested(RuntimeRequestId request);
    void playbackPauseRequested(RuntimeRequestId request);
    void playbackStopRequested(RuntimeRequestId request);
    void playbackSeekRequested(SessionTimestamp position, RuntimeRequestId request);
    void playbackRateChangeRequested(double rate, RuntimeRequestId request);
    void playbackRepeatChangeRequested(bool enabled, RuntimeRequestId request);
    void playbackMetricsResetRequested(RuntimeRequestId request);

    void constructionFailureQueued(const QString &message);

  private slots:
    void publishConstructionFailure(const QString &message);

  private:
    QThread m_networkThread;
    QThread m_sessionThread;
    QThread m_persistenceThread;
    NetworkRuntimeWorker *m_networkWorker = nullptr;
    RecordingRuntimeWorker *m_recordingWorker = nullptr;
    PlaybackRuntimeWorker *m_playbackWorker = nullptr;
    PersistenceRuntimeWorker *m_persistenceWorker = nullptr;
    RuntimeStateSource m_activeStateSource = RuntimeStateSource::None;
    quint64 m_activeStateGeneration = 0;
    quint64 m_nextStateGeneration = 0;
    bool m_shuttingDown = false;
};
