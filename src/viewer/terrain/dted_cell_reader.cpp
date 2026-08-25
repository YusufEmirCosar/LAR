#include "viewer/terrain/dted_cell_reader.h"

#include <QByteArray>
#include <QFile>
#include <QtEndian>

#include <cmath>
#include <limits>

namespace {

constexpr qsizetype UhlLength = 80;
constexpr qsizetype DsiLength = 648;
constexpr qsizetype AccLength = 2700;
constexpr qsizetype HeaderLength = UhlLength + DsiLength + AccLength;

DtedCellReadResult failure(const QString &path, DtedLevel level, const QString &reason) {
    return {nullptr, QStringLiteral("%1 tile '%2' is invalid: %3")
                         .arg(dtedLevelDisplayName(level), path, reason)};
}

bool parseUnsigned(const QByteArray &bytes, qsizetype offset, qsizetype length, int *value) {
    if (value == nullptr || offset < 0 || length <= 0 || offset + length > bytes.size()) {
        return false;
    }
    int parsed = 0;
    for (qsizetype index = 0; index < length; ++index) {
        const char character = bytes.at(offset + index);
        if (character < '0' || character > '9') {
            return false;
        }
        if (parsed > (std::numeric_limits<int>::max() - 9) / 10) {
            return false;
        }
        parsed = parsed * 10 + static_cast<int>(character - '0');
    }
    *value = parsed;
    return true;
}

bool parseOrigin(const QByteArray &bytes, qsizetype offset, double *degrees) {
    int wholeDegrees = 0;
    int minutes = 0;
    int seconds = 0;
    if (degrees == nullptr || !parseUnsigned(bytes, offset, 3, &wholeDegrees) ||
        !parseUnsigned(bytes, offset + 3, 2, &minutes) ||
        !parseUnsigned(bytes, offset + 5, 2, &seconds) || minutes >= 60 || seconds >= 60) {
        return false;
    }
    const char hemisphere = bytes.at(offset + 7);
    if (hemisphere != 'E' && hemisphere != 'W' && hemisphere != 'N' && hemisphere != 'S') {
        return false;
    }
    double result = static_cast<double>(wholeDegrees) + static_cast<double>(minutes) / 60.0 +
                    static_cast<double>(seconds) / 3600.0;
    if (hemisphere == 'W' || hemisphere == 'S') {
        result = -result;
    }
    *degrees = result;
    return true;
}

quint16 bigEndianU16(const char *data) noexcept {
    return qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(data));
}

quint32 bigEndianU32(const char *data) noexcept {
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data));
}

qint16 decodeSignedMagnitude(quint16 encoded) noexcept {
    if (encoded == 0xFFFFU) {
        return DtedCell::NoDataElevation;
    }
    if ((encoded & 0x8000U) == 0U) {
        return static_cast<qint16>(encoded);
    }
    const int magnitude = static_cast<int>(encoded & 0x7FFFU);
    const int signedMagnitude = -magnitude;
    // A small number of producers wrote two's-complement negatives. Values below the physical
    // range of Earth expose that variant unambiguously while preserving standard DTED data.
    if (signedMagnitude < -12000) {
        return static_cast<qint16>(encoded);
    }
    return static_cast<qint16>(signedMagnitude);
}

} // namespace

DtedCellReadResult DtedCellReader::readFile(const QString &path, DtedLevel expectedLevel) {
    const int maximumSamples = dtedLatitudeSampleCount(expectedLevel);
    const int expectedLatitudeInterval = dtedLatitudeIntervalTenthsArcSecond(expectedLevel);
    const qint64 maximumFileBytes = dtedMaximumFileBytes(expectedLevel);
    if (maximumSamples <= 0 || expectedLatitudeInterval <= 0 || maximumFileBytes <= 0) {
        return {nullptr, QStringLiteral("Unsupported DTED level requested for '%1'.").arg(path)};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {nullptr, QStringLiteral("%1 tile '%2' could not be opened: %3")
                             .arg(dtedLevelDisplayName(expectedLevel), path, file.errorString())};
    }
    const qint64 fileSize = file.size();
    if (fileSize < HeaderLength || fileSize > maximumFileBytes) {
        return failure(path, expectedLevel,
                       QStringLiteral("file size is outside the selected level limits"));
    }
    const QByteArray header = file.read(HeaderLength);
    if (header.size() != HeaderLength) {
        return failure(path, expectedLevel, QStringLiteral("header could not be read completely"));
    }
    if (header.first(4) != QByteArrayLiteral("UHL1") ||
        header.mid(UhlLength, 3) != QByteArrayLiteral("DSI") ||
        header.mid(UhlLength + DsiLength, 3) != QByteArrayLiteral("ACC")) {
        return failure(path, expectedLevel,
                       QStringLiteral("required UHL/DSI/ACC headers are missing"));
    }

    double longitudeOrigin = 0.0;
    double latitudeOrigin = 0.0;
    int longitudeIntervalTenths = 0;
    int latitudeIntervalTenths = 0;
    int longitudeCount = 0;
    int latitudeCount = 0;
    if (!parseOrigin(header, 4, &longitudeOrigin) || !parseOrigin(header, 12, &latitudeOrigin) ||
        !parseUnsigned(header, 20, 4, &longitudeIntervalTenths) ||
        !parseUnsigned(header, 24, 4, &latitudeIntervalTenths) ||
        !parseUnsigned(header, 47, 4, &longitudeCount) ||
        !parseUnsigned(header, 51, 4, &latitudeCount)) {
        return failure(path, expectedLevel,
                       QStringLiteral("UHL coordinate or dimension fields are malformed"));
    }
    if (longitudeCount < 2 || longitudeCount > maximumSamples || latitudeCount != maximumSamples ||
        longitudeIntervalTenths <= 0 || latitudeIntervalTenths != expectedLatitudeInterval) {
        return failure(
            path, expectedLevel,
            QStringLiteral("declared dimensions or spacing do not match the selected level"));
    }
    const int longitudeIntervalMultiplier = longitudeIntervalTenths / expectedLatitudeInterval;
    const bool standardLongitudeInterval =
        longitudeIntervalTenths % expectedLatitudeInterval == 0 &&
        (longitudeIntervalMultiplier == 1 || longitudeIntervalMultiplier == 2 ||
         longitudeIntervalMultiplier == 3 || longitudeIntervalMultiplier == 4 ||
         longitudeIntervalMultiplier == 6);
    if (!standardLongitudeInterval) {
        return failure(path, expectedLevel,
                       QStringLiteral("longitude spacing is not valid for the selected level"));
    }
    const double longitudeSpanDegrees = static_cast<double>(longitudeCount - 1) *
                                        static_cast<double>(longitudeIntervalTenths) / 36000.0;
    const double latitudeSpanDegrees = static_cast<double>(latitudeCount - 1) *
                                       static_cast<double>(latitudeIntervalTenths) / 36000.0;
    if (std::abs(longitudeSpanDegrees - 1.0) > 1.0e-9 ||
        std::abs(latitudeSpanDegrees - 1.0) > 1.0e-9) {
        return failure(path, expectedLevel,
                       QStringLiteral("sample intervals do not span exactly one degree"));
    }
    const double roundedLongitude = std::round(longitudeOrigin);
    const double roundedLatitude = std::round(latitudeOrigin);
    if (std::abs(longitudeOrigin - roundedLongitude) > 1.0e-9 ||
        std::abs(latitudeOrigin - roundedLatitude) > 1.0e-9 || roundedLongitude < -180.0 ||
        roundedLongitude > 179.0 || roundedLatitude < -90.0 || roundedLatitude > 89.0) {
        return failure(path, expectedLevel,
                       QStringLiteral("cell origin is not a valid degree boundary"));
    }

    const qint64 recordLength = 8 + static_cast<qint64>(latitudeCount) * 2 + 4;
    const qint64 expectedLength =
        static_cast<qint64>(HeaderLength) + static_cast<qint64>(longitudeCount) * recordLength;
    if (expectedLength != fileSize) {
        return failure(path, expectedLevel,
                       QStringLiteral("profile records do not match declared dimensions"));
    }

    auto cell = std::make_shared<DtedCell>();
    cell->key = {static_cast<int>(roundedLongitude), static_cast<int>(roundedLatitude)};
    cell->level = expectedLevel;
    cell->longitudeSampleCount = longitudeCount;
    cell->latitudeSampleCount = latitudeCount;
    cell->longitudeIntervalDegrees = static_cast<double>(longitudeIntervalTenths) / 36000.0;
    cell->latitudeIntervalDegrees = static_cast<double>(latitudeIntervalTenths) / 36000.0;
    cell->elevations.resize(static_cast<std::size_t>(longitudeCount) *
                            static_cast<std::size_t>(latitudeCount));

    for (int longitudeIndex = 0; longitudeIndex < longitudeCount; ++longitudeIndex) {
        const QByteArray record = file.read(recordLength);
        if (static_cast<qint64>(record.size()) != recordLength) {
            return failure(path, expectedLevel, QStringLiteral("profile record is truncated"));
        }
        if (static_cast<uchar>(record.at(0)) != 0xAAU) {
            return failure(path, expectedLevel, QStringLiteral("profile sentinel is missing"));
        }
        quint32 calculatedChecksum = 0U;
        for (qsizetype index = 0; index < record.size() - 4; ++index) {
            calculatedChecksum += static_cast<uchar>(record.at(index));
        }
        const quint32 storedChecksum = bigEndianU32(record.constData() + record.size() - 4);
        if (calculatedChecksum != storedChecksum) {
            return failure(path, expectedLevel, QStringLiteral("profile checksum mismatch"));
        }
        for (int latitudeIndex = 0; latitudeIndex < latitudeCount; ++latitudeIndex) {
            const quint16 encoded =
                bigEndianU16(record.constData() + 8 + static_cast<qsizetype>(latitudeIndex) * 2);
            const std::size_t offset =
                static_cast<std::size_t>(longitudeIndex) * static_cast<std::size_t>(latitudeCount) +
                static_cast<std::size_t>(latitudeIndex);
            cell->elevations[offset] = decodeSignedMagnitude(encoded);
        }
    }
    if (!cell->valid()) {
        return failure(path, expectedLevel, QStringLiteral("decoded cell is inconsistent"));
    }
    return {std::move(cell), {}};
}
