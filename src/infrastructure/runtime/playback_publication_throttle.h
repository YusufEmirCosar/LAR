#pragma once

/**
 * @file playback_publication_throttle.h
 * @brief Coalesces high-rate playback frames before main-thread delivery.
 */

#include "application/session_timestamp.h"
#include "domain/decoded_state.h"

#include <QBitArray>
#include <QObject>
#include <QTimer>

/**
 * Coalesces high-rate playback frames and positions for presentation.
 */
class PlaybackPublicationThrottle final : public QObject {
    Q_OBJECT

  public:
    explicit PlaybackPublicationThrottle(QObject *parent = nullptr);

    void start();
    void stop();
    void clear();

  public slots:
    void captureFrame(const DecodedState &state);
    void capturePosition(SessionTimestamp position);
    /**
     * @brief Publishes the latest coalesced state and position.
     */
    void publish();

  signals:
    void stateReady(const DecodedState &state);
    void positionChanged(SessionTimestamp position);

  private:
    QTimer *m_timer = nullptr;
    DecodedState m_latestState;
    bool m_hasLatestFrame = false;
    bool m_frameDirty = false;
    SessionTimestamp m_latestPosition;
    bool m_positionDirty = false;
};
