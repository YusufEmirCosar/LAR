
#include "infrastructure/runtime/playback_metrics.h"

#include <cmath>

PlaybackMetrics::PlaybackMetrics(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &PlaybackMetrics::publish);
}

void PlaybackMetrics::record(quint64 count) {
    m_processedPackets += count;
    m_packetsInRateWindow += count;
}

void PlaybackMetrics::setActive(bool active) {
    if (active == m_active)
        return;
    m_active = active;
    if (active) {
        m_packetsInRateWindow = 0;
        m_clock.restart();
        m_timer->start();
    } else {
        m_timer->stop();
    }
}

void PlaybackMetrics::reset() {
    m_processedPackets = 0;
    m_packetsInRateWindow = 0;
    m_clock.restart();
    emit metricsChanged(0, 0);
}

void PlaybackMetrics::finish() {
    publish();
    setActive(false);
}

void PlaybackMetrics::shutdown() {
    m_timer->stop();
    m_active = false;
}

void PlaybackMetrics::publish() {
    const qint64 elapsedMs = qMax<qint64>(1, m_clock.isValid() ? m_clock.elapsed() : 1000);
    const quint64 rate =
        quint64(std::llround(double(m_packetsInRateWindow) * 1000.0 / double(elapsedMs)));
    emit metricsChanged(m_processedPackets, rate);
    m_packetsInRateWindow = 0;
    m_clock.restart();
}
