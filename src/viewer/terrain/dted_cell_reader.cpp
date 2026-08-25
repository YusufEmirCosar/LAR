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
constexpr int MaximumLevelZeroSamples = 121;
constexpr qint64 MaximumLevelZeroFileBytes = 128 * 1024;

DtedCellReadResult failure(const QString &path, const QString &reason) {
    return {nullptr, QStringLiteral("DTED0 tile '%1' is invalid: %2").arg(path, reason)};
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

DtedCellReadResult DtedCellReader::readFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {nullptr, QStringLiteral("DTED0 tile '%1' could not be opened: %2")
                             .arg(path, file.errorString())};
    }
    const qint64 fileSize = file.size();
    if (fileSize < HeaderLength || fileSize > MaximumLevelZeroFileBytes) {
        return failure(path, QStringLiteral("file size is outside Level-0 limits"));
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() != fileSize || bytes.size() < HeaderLength) {
        return failure(path, QStringLiteral("file could not be read completely"));
    }
    if (bytes.first(4) != QByteArrayLiteral("UHL1") ||
        bytes.mid(UhlLength, 3) != QByteArrayLiteral("DSI") ||
        bytes.mid(UhlLength + DsiLength, 3) != QByteArrayLiteral("ACC")) {
        return failure(path, QStringLiteral("required UHL/DSI/ACC headers are missing"));
    }

    double longitudeOrigin = 0.0;
    double latitudeOrigin = 0.0;
    int longitudeIntervalTenths = 0;
    int latitudeIntervalTenths = 0;
    int longitudeCount = 0;
    int latitudeCount = 0;
    if (!parseOrigin(bytes, 4, &longitudeOrigin) || !parseOrigin(bytes, 12, &latitudeOrigin) ||
        !parseUnsigned(bytes, 20, 4, &longitudeIntervalTenths) ||
        !parseUnsigned(bytes, 24, 4, &latitudeIntervalTenths) ||
        !parseUnsigned(bytes, 47, 4, &longitudeCount) ||
        !parseUnsigned(bytes, 51, 4, &latitudeCount)) {
        return failure(path, QStringLiteral("UHL coordinate or dimension fields are malformed"));
    }
    if (longitudeCount < 2 || longitudeCount > MaximumLevelZeroSamples || latitudeCount < 2 ||
        latitudeCount > MaximumLevelZeroSamples || longitudeIntervalTenths <= 0 ||
        latitudeIntervalTenths <= 0) {
        return failure(path, QStringLiteral("declared dimensions exceed Level-0 limits"));
    }
    const double longitudeSpanDegrees = static_cast<double>(longitudeCount - 1) *
                                        static_cast<double>(longitudeIntervalTenths) / 36000.0;
    const double latitudeSpanDegrees = static_cast<double>(latitudeCount - 1) *
                                       static_cast<double>(latitudeIntervalTenths) / 36000.0;
    if (std::abs(longitudeSpanDegrees - 1.0) > 1.0e-9 ||
        std::abs(latitudeSpanDegrees - 1.0) > 1.0e-9) {
        return failure(path, QStringLiteral("sample intervals do not span exactly one degree"));
    }
    const double roundedLongitude = std::round(longitudeOrigin);
    const double roundedLatitude = std::round(latitudeOrigin);
    if (std::abs(longitudeOrigin - roundedLongitude) > 1.0e-9 ||
        std::abs(latitudeOrigin - roundedLatitude) > 1.0e-9 || roundedLongitude < -180.0 ||
        roundedLongitude > 179.0 || roundedLatitude < -90.0 || roundedLatitude > 89.0) {
        return failure(path, QStringLiteral("cell origin is not a valid degree boundary"));
    }

    const qsizetype recordLength = 8 + static_cast<qsizetype>(latitudeCount) * 2 + 4;
    const qsizetype expectedLength =
        HeaderLength + static_cast<qsizetype>(longitudeCount) * recordLength;
    if (expectedLength != bytes.size()) {
        return failure(path, QStringLiteral("profile records do not match declared dimensions"));
    }

    auto cell = std::make_shared<DtedCell>();
    cell->key = {static_cast<int>(roundedLongitude), static_cast<int>(roundedLatitude)};
    cell->longitudeSampleCount = longitudeCount;
    cell->latitudeSampleCount = latitudeCount;
    cell->longitudeIntervalDegrees = static_cast<double>(longitudeIntervalTenths) / 36000.0;
    cell->latitudeIntervalDegrees = static_cast<double>(latitudeIntervalTenths) / 36000.0;
    cell->elevations.reserve(static_cast<std::size_t>(longitudeCount) *
                             static_cast<std::size_t>(latitudeCount));

    for (int longitudeIndex = 0; longitudeIndex < longitudeCount; ++longitudeIndex) {
        const qsizetype recordOffset =
            HeaderLength + static_cast<qsizetype>(longitudeIndex) * recordLength;
        if (static_cast<uchar>(bytes.at(recordOffset)) != 0xAAU) {
            return failure(path, QStringLiteral("profile sentinel is missing"));
        }
        quint32 calculatedChecksum = 0U;
        for (qsizetype index = 0; index < recordLength - 4; ++index) {
            calculatedChecksum += static_cast<uchar>(bytes.at(recordOffset + index));
        }
        const quint32 storedChecksum =
            bigEndianU32(bytes.constData() + recordOffset + recordLength - 4);
        if (calculatedChecksum != storedChecksum) {
            return failure(path, QStringLiteral("profile checksum mismatch"));
        }
        const qsizetype sampleOffset = recordOffset + 8;
        for (int latitudeIndex = 0; latitudeIndex < latitudeCount; ++latitudeIndex) {
            const quint16 encoded =
                bigEndianU16(bytes.constData() + sampleOffset + latitudeIndex * 2);
            cell->elevations.push_back(decodeSignedMagnitude(encoded));
        }
    }
    if (!cell->valid()) {
        return failure(path, QStringLiteral("decoded cell is inconsistent"));
    }
    return {std::move(cell), {}};
}
