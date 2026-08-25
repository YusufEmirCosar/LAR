#pragma once

/**
 * @file direct_application_runtime.h
 * @brief Synchronous IApplicationRuntime implementation for tests/embedding.
 */

#include "application/metrics_service.h"
#include "application/online_capture_service.h"
#include "application/playback_service.h"
#include "application/ports/application_runtime.h"
#include "application/ports/mapping_repository.h"
#include "application/ports/session_persistence.h"
#include "application/recording_service.h"

/**
 * Immediate runtime adapter used by unit tests and embedders that deliberately
 * keep every service on one thread.
 */
class DirectApplicationRuntime final : public IApplicationRuntime {
    Q_OBJECT

  public:
    DirectApplicationRuntime(OnlineCaptureService &capture, RecordingService &recording,
                             PlaybackService &playback, MetricsService &metrics,
                             IMappingRepository &mappingRepository,
                             ISessionPersistence &sessionPersistence, QObject *parent = nullptr);

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
    /**
     * @brief Shuts down the direct runtime.
     */
    void shutdown() override;

  private:
    void publishMetrics();
    void publishRecordingState();

    OnlineCaptureService &m_capture;
    RecordingService &m_recording;
    PlaybackService &m_playback;
    MetricsService &m_metrics;
    IMappingRepository &m_mappingRepository;
    ISessionPersistence &m_sessionPersistence;
    PacketMapping m_mapping;
    IpAccessPolicy m_ipPolicy;
    quint64 m_rate = 0;
    RuntimeStateSource m_activeStateSource = RuntimeStateSource::None;
    quint64 m_activeStateGeneration = 0;
    quint64 m_nextStateGeneration = 0;
    bool m_shuttingDown = false;
    bool m_hasPublishedRecordingState = false;
    bool m_lastHasRecordingSession = false;
    bool m_lastRecordingPaused = false;
    quint64 m_lastRecordCount = 0;
    SessionTimestamp m_lastRecordingDuration;
};
