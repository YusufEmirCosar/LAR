
#include "viewer/playbacktimeformatter.h"

#include <QtGlobal>

QString PlaybackTimeFormatter::format(SessionTimestamp timestamp) {
    const qint64 milliseconds = timestamp.milliseconds();
    const qint64 days = milliseconds / 86400000;
    const qint64 hours = (milliseconds / 3600000) % 24;
    const qint64 totalHours = milliseconds / 3600000;
    const qint64 minutes = (milliseconds / 60000) % 60;
    const qint64 seconds = (milliseconds / 1000) % 60;
    const qint64 millis = milliseconds % 1000;
    if (days > 0) {
        return QStringLiteral("%1d %2:%3:%4.%5")
            .arg(days)
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'))
            .arg(millis, 3, 10, QLatin1Char('0'));
    }
    if (totalHours > 0) {
        return QStringLiteral("%1:%2:%3.%4")
            .arg(totalHours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'))
            .arg(millis, 3, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2.%3")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(millis, 3, 10, QLatin1Char('0'));
}
