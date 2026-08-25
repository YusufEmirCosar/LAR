#pragma once

/**
 * @file playback_clock.h
 * @brief Timer port used by PlaybackService's fixed-rate replay policy.
 */

#include <QObject>

/** @brief Emits playback ticks at the frame rate selected by replay policy. */
class IPlaybackClock : public QObject {
    Q_OBJECT

  public:
    explicit IPlaybackClock(QObject *parent = nullptr) : QObject(parent) {}
    ~IPlaybackClock() override = default;

    virtual void start(int framesPerSecond) = 0;
    /**
     * @brief Stops future ticks.
     */
    virtual void stop() = 0;
    /**
     * @brief Reports whether ticks are currently scheduled.
     *
     * @return True when the reported condition holds; false otherwise.
     */
    virtual bool isActive() const = 0;

  signals:
    void tick();
};
