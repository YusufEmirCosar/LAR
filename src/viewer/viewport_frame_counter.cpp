
#include "viewer/viewport_frame_counter.h"

#include <cmath>

ViewportFrameCounter::ViewportFrameCounter(QObject *parent) : QObject(parent) {
    m_fpsTimer.setInterval(1000);
    m_fpsTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_fpsTimer, &QTimer::timeout, this, [this] {
        const qint64 elapsedMilliseconds = qMax<qint64>(1, m_sampleClock.restart());
        m_fps = static_cast<int>(
            std::llround(double(m_drawOperations) * 1000.0 / double(elapsedMilliseconds)));
        emit framesPerSecondChanged(m_fps);
        m_drawOperations = 0;
    });
    m_sampleClock.start();
    m_fpsTimer.start();
}

void ViewportFrameCounter::recordFrame() {
    ++m_drawOperations;
    ++m_totalFrames;
    emit totalFrameCountChanged(m_totalFrames);
}

void ViewportFrameCounter::resetTotalCount() {
    m_totalFrames = 0;
    emit totalFrameCountChanged(0);
}
