#pragma once

/**
 * @file metrics_service.h
 * @brief Processed-packet totals and rolling throughput calculation.
 */

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

/** @brief Counts packet attempts and publishes a once-per-second rate. */
class MetricsService final : public QObject {
    Q_OBJECT

  public:
    explicit MetricsService(QObject *parent = nullptr);

    quint64 totalProcessedPackets() const noexcept {
        return m_totalProcessed;
    }
    quint64 packetsPerSecond() const noexcept {
        return m_currentRate;
    }

    void recordDatagramAttempted();
    void recordPacketProcessed();
    void recordPlaybackPackets(quint64 count);
    void reset();
    /** @brief Stops rate publication before the owning worker is rehomed. */
    void shutdown();

  signals:
    void processedCountChanged(quint64 total);
    void rateChanged(quint64 ratePerSecond);

  private:
    /**
     * @brief Calculates rate.
     */
    void calculateRate();

    quint64 m_totalProcessed = 0;
    quint64 m_intervalAttempts = 0;
    quint64 m_currentRate = 0;
    QTimer *m_timer = nullptr;
    QElapsedTimer m_clock;
};
