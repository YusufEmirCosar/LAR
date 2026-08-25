#pragma once

/**
 * @file playback_metrics.h
 * @brief Throughput accounting for records published during playback.
 */

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

/**
 * Owns playback-only throughput counters and rate windows.
 */
class PlaybackMetrics final : public QObject {
    Q_OBJECT

  public:
    explicit PlaybackMetrics(QObject *parent = nullptr);

    void record(quint64 count);
    void setActive(bool active);
    void reset();
    /**
     * @brief Finalizes metrics and stops the rate timer.
     */
    void finish();
    /**
     * @brief Shuts down metrics and stops the rate timer.
     */
    void shutdown();

  signals:
    void metricsChanged(quint64 processedCount, quint64 packetsPerSecond);

  private:
    /**
     * @brief Publishes the current playback metrics.
     */
    void publish();

    QTimer *m_timer = nullptr;
    QElapsedTimer m_clock;
    quint64 m_processedPackets = 0;
    quint64 m_packetsInRateWindow = 0;
    bool m_active = false;
};
