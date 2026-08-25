
#include "viewer/viewport_frame_counter.h"

ViewportFrameCounter::ViewportFrameCounter(QObject *parent) : QObject(parent) {
    m_fpsTimer.setInterval(1000);
    connect(&m_fpsTimer, &QTimer::timeout, this, [this] {
        m_fps = m_drawOperations;
        emit framesPerSecondChanged(m_fps);
        m_drawOperations = 0;
    });
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
