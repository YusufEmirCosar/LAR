
#include "application/metrics_service.h"

#include <cmath>

MetricsService::MetricsService(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &MetricsService::calculateRate);
    m_timer->start();
    m_clock.start();
}

void MetricsService::shutdown() {
    m_timer->stop();
}

void MetricsService::recordDatagramAttempted() {
    ++m_intervalAttempts;
}

void MetricsService::recordPacketProcessed() {
    ++m_totalProcessed;
    emit processedCountChanged(m_totalProcessed);
}

void MetricsService::recordPlaybackPackets(quint64 count) {
    if (count == 0)
        return;
    m_totalProcessed += count;
    m_intervalAttempts += count;
    emit processedCountChanged(m_totalProcessed);
}

void MetricsService::reset() {
    m_totalProcessed = 0;
    m_intervalAttempts = 0;
    m_currentRate = 0;
    m_clock.restart();
    emit processedCountChanged(0);
    emit rateChanged(0);
}

void MetricsService::calculateRate() {
    const qint64 elapsedMs = m_clock.restart();
    if (elapsedMs > 0) {
        m_currentRate = static_cast<quint64>(std::round(
            (static_cast<double>(m_intervalAttempts) * 1000.0) / static_cast<double>(elapsedMs)));
    } else {
        m_currentRate = m_intervalAttempts;
    }
    m_intervalAttempts = 0;
    emit rateChanged(m_currentRate);
}
