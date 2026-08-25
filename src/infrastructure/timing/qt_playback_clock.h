#pragma once

/**
 * @file qt_playback_clock.h
 * @brief Precise QTimer playback-clock adapter.
 */

#include "application/ports/playback_clock.h"
#include <QElapsedTimer>
#include <QTimer>

#include <algorithm>
#include <limits>

/** @brief Precise Qt timer that supplies fixed-rate playback ticks. */
class QtPlaybackClock final : public IPlaybackClock {
    Q_OBJECT

  public:
    explicit QtPlaybackClock(QObject *parent = nullptr) : IPlaybackClock(parent) {
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::PreciseTimer);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, [this] {
            if (!m_active)
                return;
            emit tick();
            if (m_active) {
                ++m_nextTick;
                scheduleNext();
            }
        });
    }

    void start(int framesPerSecond) override {
        m_timer->stop();
        m_framesPerSecond = std::max(1, framesPerSecond);
        m_nextTick = 1;
        m_elapsed.start();
        m_active = true;
        scheduleNext();
    }

    void stop() override {
        m_active = false;
        m_timer->stop();
    }

    bool isActive() const override {
        return m_active;
    }

  private:
    void scheduleNext() {
        constexpr qint64 NanosecondsPerSecond = 1'000'000'000;
        constexpr qint64 NanosecondsPerMillisecond = 1'000'000;
        const qint64 maximumTick = std::numeric_limits<qint64>::max() / NanosecondsPerSecond;
        const qint64 boundedTick = std::min(m_nextTick, maximumTick);
        const qint64 targetNanoseconds =
            (boundedTick * NanosecondsPerSecond + m_framesPerSecond - 1) / m_framesPerSecond;
        const qint64 remainingNanoseconds =
            std::max<qint64>(0, targetNanoseconds - m_elapsed.nsecsElapsed());
        const qint64 delayMilliseconds =
            (remainingNanoseconds + NanosecondsPerMillisecond - 1) / NanosecondsPerMillisecond;
        m_timer->start(
            static_cast<int>(std::min<qint64>(delayMilliseconds, std::numeric_limits<int>::max())));
    }

    QTimer *m_timer = nullptr;
    QElapsedTimer m_elapsed;
    qint64 m_nextTick = 0;
    int m_framesPerSecond = 1;
    bool m_active = false;
};
