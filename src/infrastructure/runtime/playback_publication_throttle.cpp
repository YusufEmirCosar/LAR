
#include "infrastructure/runtime/playback_publication_throttle.h"

PlaybackPublicationThrottle::PlaybackPublicationThrottle(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &PlaybackPublicationThrottle::publish);
}

void PlaybackPublicationThrottle::start() {
    m_timer->start();
}

void PlaybackPublicationThrottle::stop() {
    m_timer->stop();
}

void PlaybackPublicationThrottle::clear() {
    m_latestState = {};
    m_hasLatestFrame = false;
    m_frameDirty = false;
    m_latestPosition = {};
    m_positionDirty = false;
}

void PlaybackPublicationThrottle::captureFrame(const DecodedState &state) {
    m_latestState = state;
    m_hasLatestFrame = true;
    m_frameDirty = true;
}

void PlaybackPublicationThrottle::capturePosition(SessionTimestamp position) {
    m_latestPosition = position;
    m_positionDirty = true;
}

void PlaybackPublicationThrottle::publish() {
    if (m_hasLatestFrame && m_frameDirty) {
        m_frameDirty = false;
        emit stateReady(m_latestState);
    }
    if (m_positionDirty) {
        m_positionDirty = false;
        emit positionChanged(m_latestPosition);
    }
}
