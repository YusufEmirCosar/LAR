#include "viewer/terrain/dted_water_mask_source.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>

#include <limits>
#include <utility>

namespace {

constexpr qint64 HeaderBytes = 32;
constexpr qint64 EntryBytes = 16;
constexpr std::size_t CellCount = 360U * 180U;
constexpr qint64 MaximumPackBytes = 128 * 1024 * 1024;
constexpr quint16 FormatVersion = 1U;
constexpr quint32 CrcPolynomial = 0xEDB88320U;

quint16 littleU16(const char *data) noexcept {
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data));
}

quint32 littleU32(const char *data) noexcept {
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data));
}

quint64 littleU64(const char *data) noexcept {
    return qFromLittleEndian<quint64>(reinterpret_cast<const uchar *>(data));
}

quint32 crc32(const QByteArray &bytes) noexcept {
    quint32 result = std::numeric_limits<quint32>::max();
    for (const char character : bytes) {
        result ^= static_cast<quint8>(character);
        for (int bit = 0; bit < 8; ++bit) {
            const quint32 mask = 0U - (result & 1U);
            result = (result >> 1U) ^ (CrcPolynomial & mask);
        }
    }
    return result ^ std::numeric_limits<quint32>::max();
}

QString invalidPack(const QString &path, const QString &reason) {
    return QStringLiteral("DTED water-mask pack '%1' is invalid: %2").arg(path, reason);
}

} // namespace

DtedWaterMaskSource::DtedWaterMaskSource(QString packPath) : m_packPath(std::move(packPath)) {
    initialize();
}

std::size_t DtedWaterMaskSource::indexFor(const DtedCellKey &key) noexcept {
    if (key.longitudeDegrees < -180 || key.longitudeDegrees > 179 || key.latitudeDegrees < -90 ||
        key.latitudeDegrees > 89) {
        return CellCount;
    }
    return static_cast<std::size_t>(key.latitudeDegrees + 90) * 360U +
           static_cast<std::size_t>(key.longitudeDegrees + 180);
}

void DtedWaterMaskSource::initialize() {
    m_entries.clear();
    m_initializationError.clear();
    if (m_packPath.isEmpty()) {
        return;
    }
    QFile file(m_packPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_initializationError = QStringLiteral("DTED water-mask pack '%1' could not be opened: %2")
                                    .arg(m_packPath, file.errorString());
        return;
    }
    const qint64 fileSize = file.size();
    const qint64 minimumBytes = HeaderBytes + static_cast<qint64>(CellCount) * EntryBytes;
    if (fileSize < minimumBytes || fileSize > MaximumPackBytes) {
        m_initializationError =
            invalidPack(m_packPath, QStringLiteral("file size is out of bounds"));
        return;
    }
    const QByteArray header = file.read(HeaderBytes);
    if (header.size() != HeaderBytes || header.first(8) != QByteArrayLiteral("LARWMSK1")) {
        m_initializationError = invalidPack(m_packPath, QStringLiteral("header magic is missing"));
        return;
    }
    const quint16 version = littleU16(header.constData() + 8);
    const quint16 headerSize = littleU16(header.constData() + 10);
    const quint32 cellCount = littleU32(header.constData() + 12);
    const quint16 entrySize = littleU16(header.constData() + 16);
    const quint16 reserved = littleU16(header.constData() + 18);
    const quint32 payloadOffset = littleU32(header.constData() + 20);
    const quint64 declaredFileSize = littleU64(header.constData() + 24);
    const quint32 expectedPayloadOffset =
        static_cast<quint32>(HeaderBytes + static_cast<qint64>(CellCount) * EntryBytes);
    if (version != FormatVersion || headerSize != HeaderBytes || cellCount != CellCount ||
        entrySize != EntryBytes || reserved != 0U || payloadOffset != expectedPayloadOffset ||
        declaredFileSize != static_cast<quint64>(fileSize)) {
        m_initializationError =
            invalidPack(m_packPath, QStringLiteral("header fields do not match format version 1"));
        return;
    }

    const QByteArray indexBytes = file.read(static_cast<qint64>(CellCount) * EntryBytes);
    if (indexBytes.size() != static_cast<qint64>(CellCount) * EntryBytes) {
        m_initializationError = invalidPack(m_packPath, QStringLiteral("index is truncated"));
        return;
    }
    std::vector<IndexEntry> entries;
    entries.reserve(CellCount);
    quint64 nextPayloadOffset = payloadOffset;
    for (std::size_t index = 0; index < CellCount; ++index) {
        const char *encoded = indexBytes.constData() + static_cast<qint64>(index) * EntryBytes;
        const quint8 state = static_cast<quint8>(encoded[0]);
        IndexEntry entry;
        entry.coverage = static_cast<DtedWaterCoverage>(state);
        entry.longitudeSampleCount = static_cast<quint8>(encoded[1]);
        entry.latitudeSampleCount = static_cast<quint8>(encoded[2]);
        const quint8 entryReserved = static_cast<quint8>(encoded[3]);
        entry.payloadOffset = littleU32(encoded + 4);
        entry.payloadBytes = littleU32(encoded + 8);
        entry.checksum = littleU32(encoded + 12);
        const bool recognizedState = state <= static_cast<quint8>(DtedWaterCoverage::Mixed);
        const bool missing = entry.coverage == DtedWaterCoverage::Missing;
        const bool dimensionsValid =
            missing ? entry.longitudeSampleCount == 0U && entry.latitudeSampleCount == 0U
                    : entry.longitudeSampleCount >= 2U && entry.longitudeSampleCount <= 121U &&
                          entry.latitudeSampleCount >= 2U && entry.latitudeSampleCount <= 121U;
        const std::size_t sampleCount = static_cast<std::size_t>(entry.longitudeSampleCount) *
                                        static_cast<std::size_t>(entry.latitudeSampleCount);
        const quint32 expectedPayloadBytes = static_cast<quint32>((sampleCount + 7U) / 8U);
        const bool mixed = entry.coverage == DtedWaterCoverage::Mixed;
        const bool payloadValid =
            mixed ? entry.payloadBytes == expectedPayloadBytes &&
                        static_cast<quint64>(entry.payloadOffset) == nextPayloadOffset &&
                        nextPayloadOffset + entry.payloadBytes <= declaredFileSize
                  : entry.payloadOffset == 0U && entry.payloadBytes == 0U && entry.checksum == 0U;
        if (!recognizedState || entryReserved != 0U || !dimensionsValid || !payloadValid) {
            m_initializationError =
                invalidPack(m_packPath, QStringLiteral("index entry %1 is malformed").arg(index));
            return;
        }
        if (mixed) {
            nextPayloadOffset += entry.payloadBytes;
        }
        entries.push_back(entry);
    }
    if (nextPayloadOffset != declaredFileSize) {
        m_initializationError =
            invalidPack(m_packPath, QStringLiteral("payload ranges do not cover the file exactly"));
        return;
    }
    m_entries = std::move(entries);
}

DtedWaterMaskReadResult DtedWaterMaskSource::load(const DtedCellKey &key) const {
    const std::size_t index = indexFor(key);
    if (!isAvailable() || index >= m_entries.size()) {
        return {nullptr, m_initializationError.isEmpty()
                             ? QStringLiteral("DTED water-mask source is unavailable.")
                             : m_initializationError};
    }
    const IndexEntry &entry = m_entries[index];
    if (entry.coverage == DtedWaterCoverage::Missing) {
        return {nullptr, QStringLiteral("DTED water-mask cell is unavailable at %1, %2.")
                             .arg(key.latitudeDegrees)
                             .arg(key.longitudeDegrees)};
    }
    auto cell = std::make_shared<DtedWaterMaskCell>();
    cell->key = key;
    cell->longitudeSampleCount = entry.longitudeSampleCount;
    cell->latitudeSampleCount = entry.latitudeSampleCount;
    cell->coverage = entry.coverage;
    if (entry.coverage == DtedWaterCoverage::Mixed) {
        QFile file(m_packPath);
        if (!file.open(QIODevice::ReadOnly) || !file.seek(entry.payloadOffset)) {
            return {nullptr, QStringLiteral("DTED water-mask payload could not be opened.")};
        }
        const QByteArray payload = file.read(entry.payloadBytes);
        if (payload.size() != entry.payloadBytes || crc32(payload) != entry.checksum) {
            return {nullptr, QStringLiteral("DTED water-mask payload is truncated or corrupt.")};
        }
        cell->packedWaterBits.assign(
            reinterpret_cast<const quint8 *>(payload.constData()),
            reinterpret_cast<const quint8 *>(payload.constData() + payload.size()));
        const std::size_t sampleCount = static_cast<std::size_t>(entry.longitudeSampleCount) *
                                        static_cast<std::size_t>(entry.latitudeSampleCount);
        const std::size_t unusedBits = cell->packedWaterBits.size() * 8U - sampleCount;
        if (unusedBits > 0U) {
            const quint8 usedMask = static_cast<quint8>(0xFFU >> unusedBits);
            if ((cell->packedWaterBits.back() & static_cast<quint8>(~usedMask)) != 0U) {
                return {nullptr,
                        QStringLiteral("DTED water-mask payload has non-zero padding bits.")};
            }
        }
    }
    if (!cell->valid()) {
        return {nullptr, QStringLiteral("DTED water-mask cell is internally inconsistent.")};
    }
    return {std::move(cell), {}};
}
