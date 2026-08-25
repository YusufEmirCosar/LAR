#pragma once

/**
 * @file captured_packet.h
 * @brief Validated UDP payloads annotated with monotonic receipt time.
 */

#include <QByteArray>
#include <QMetaType>
#include <QVector>
#include <QtTypes>

/**
 * A validated raw datagram together with its transport-receipt time.
 *
 * The timestamp is an absolute point in the process-wide monotonic clock
 * domain. RecordingService converts it to session-relative active time.
 */
struct CapturedPacket final {
    QByteArray data;
    qint64 receivedAtNanoseconds = 0;
};

Q_DECLARE_METATYPE(CapturedPacket)
Q_DECLARE_METATYPE(QVector<CapturedPacket>)
