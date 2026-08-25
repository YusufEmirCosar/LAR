#pragma once

/**
 * @file viewport_frame_counter.h
 * @brief Rolling FPS and total rendered-frame counter for viewport pages.
 */

#include <QObject>
#include <QTimer>

/** @brief Aggregates page render notifications into displayable metrics. */
class ViewportFrameCounter final : public QObject {
    Q_OBJECT

  public:
    explicit ViewportFrameCounter(QObject *parent = nullptr);

    void recordFrame();
    void resetTotalCount();

    int fps() const noexcept {
        return m_fps;
    }
    quint64 totalFrames() const noexcept {
        return m_totalFrames;
    }

  signals:
    void framesPerSecondChanged(int fps);
    void totalFrameCountChanged(quint64 totalFrames);

  private:
    QTimer m_fpsTimer;
    int m_drawOperations = 0;
    int m_fps = 0;
    quint64 m_totalFrames = 0;
};
