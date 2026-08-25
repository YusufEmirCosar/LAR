#pragma once

/**
 * @file dted_fixture.h
 * @brief Synthetic bounded DTED cell generation for parser, worker, and GPU tests.
 */

#include "viewer/terrain/dted_cell.h"
#include "viewer/terrain/dted_tile_source.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QtEndian>

#include <cmath>

namespace dted_test_fixture {

inline QByteArray origin(int degrees, bool longitude) {
    const bool negative = degrees < 0;
    const QChar hemisphere = longitude ? (negative ? QLatin1Char('W') : QLatin1Char('E'))
                                       : (negative ? QLatin1Char('S') : QLatin1Char('N'));
    return QStringLiteral("%1%2%3")
        .arg(std::abs(degrees), 3, 10, QLatin1Char('0'))
        .arg(QStringLiteral("0000"))
        .arg(hemisphere)
        .toLatin1();
}

inline QByteArray fourDigitField(int value) {
    return QStringLiteral("%1").arg(value, 4, 10, QLatin1Char('0')).toLatin1();
}

inline quint16 encodeElevation(qint16 elevation) {
    if (elevation == DtedCell::NoDataElevation) {
        return 0xFFFFU;
    }
    if (elevation >= 0) {
        return static_cast<quint16>(elevation);
    }
    return static_cast<quint16>(0x8000U | static_cast<quint16>(-static_cast<int>(elevation)));
}

inline QByteArray cellBytes(DtedLevel level, const DtedCellKey &key,
                            int longitudeIntervalMultiplier, qint16 elevation) {
    constexpr qsizetype UhlLength = 80;
    constexpr qsizetype DsiLength = 648;
    constexpr qsizetype AccLength = 2700;
    constexpr qsizetype HeaderLength = UhlLength + DsiLength + AccLength;
    const int latitudeCount = dtedLatitudeSampleCount(level);
    const int latitudeInterval = dtedLatitudeIntervalTenthsArcSecond(level);
    const int longitudeInterval = latitudeInterval * longitudeIntervalMultiplier;
    if (latitudeCount <= 0 || longitudeInterval <= 0 || 36000 % longitudeInterval != 0) {
        return {};
    }
    const int longitudeCount = 36000 / longitudeInterval + 1;
    QByteArray result(HeaderLength, ' ');
    result.replace(0, 4, QByteArrayLiteral("UHL1"));
    result.replace(4, 8, origin(key.longitudeDegrees, true));
    result.replace(12, 8, origin(key.latitudeDegrees, false));
    result.replace(20, 4, fourDigitField(longitudeInterval));
    result.replace(24, 4, fourDigitField(latitudeInterval));
    result.replace(47, 4, fourDigitField(longitudeCount));
    result.replace(51, 4, fourDigitField(latitudeCount));
    result.replace(UhlLength, 3, QByteArrayLiteral("DSI"));
    result.replace(UhlLength + DsiLength, 3, QByteArrayLiteral("ACC"));

    const qsizetype recordLength = 8 + static_cast<qsizetype>(latitudeCount) * 2 + 4;
    result.reserve(HeaderLength + static_cast<qsizetype>(longitudeCount) * recordLength);
    const quint16 encodedElevation = encodeElevation(elevation);
    for (int longitudeIndex = 0; longitudeIndex < longitudeCount; ++longitudeIndex) {
        QByteArray record(recordLength, '\0');
        record[0] = static_cast<char>(0xAAU);
        qToBigEndian<quint16>(static_cast<quint16>(longitudeIndex),
                              reinterpret_cast<uchar *>(record.data() + 2));
        for (int latitudeIndex = 0; latitudeIndex < latitudeCount; ++latitudeIndex) {
            qToBigEndian<quint16>(encodedElevation,
                                  reinterpret_cast<uchar *>(record.data() + 8 + latitudeIndex * 2));
        }
        quint32 checksum = 0U;
        for (qsizetype index = 0; index < recordLength - 4; ++index) {
            checksum += static_cast<uchar>(record.at(index));
        }
        qToBigEndian<quint32>(checksum,
                              reinterpret_cast<uchar *>(record.data() + recordLength - 4));
        result.append(record);
    }
    return result;
}

inline bool writeCell(const QString &root, DtedLevel level, const DtedCellKey &key,
                      int longitudeIntervalMultiplier, qint16 elevation) {
    const DtedTileSource source(root, level);
    const QString path = source.pathFor(key);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    const QByteArray bytes = cellBytes(level, key, longitudeIntervalMultiplier, elevation);
    QFile file(path);
    return !bytes.isEmpty() && file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

} // namespace dted_test_fixture
