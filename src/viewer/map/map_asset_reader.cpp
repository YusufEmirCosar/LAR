
#include "viewer/map/map_asset_reader.h"

#include "viewer/map/map_asset_format.h"
#include "viewer/map/map_asset_limits.h"
#include "viewer/map/map_checksum.h"
#include "viewer/map/map_land_index.h"

#include <QtEndian>

#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace lar::map {
namespace {

MapAssetReadResult failure(MapAssetError error, const QString &message) {
    return {nullptr, error, message};
}

bool checkedAdd(std::size_t first, std::size_t second, std::size_t &result) noexcept {
    if (first > std::numeric_limits<std::size_t>::max() - second) {
        return false;
    }
    result = first + second;
    return true;
}

bool checkedMultiply(std::size_t first, std::size_t second, std::size_t &result) noexcept {
    if (first != 0U && second > std::numeric_limits<std::size_t>::max() / first) {
        return false;
    }
    result = first * second;
    return true;
}

std::uint32_t readUint32(const unsigned char *bytes) noexcept {
    return qFromLittleEndian<std::uint32_t>(bytes);
}

std::uint64_t readUint64(const unsigned char *bytes) noexcept {
    return qFromLittleEndian<std::uint64_t>(bytes);
}

bool validCounts(std::uint64_t vertexCount, std::uint64_t mercatorIndexCount,
                 std::uint64_t sphereIndexCount, std::uint64_t borderIndexCount,
                 std::uint64_t landCellCount, std::uint64_t landReferenceCount) noexcept {
    const bool validLandCounts = (landCellCount == 0U && landReferenceCount == 0U) ||
                                 (landCellCount == MapLandCellCount &&
                                  landReferenceCount <= limits::MaximumLandTriangleReferenceCount);
    return vertexCount > 0U && vertexCount <= limits::MaximumVertexCount &&
           mercatorIndexCount > 0U && mercatorIndexCount <= limits::MaximumMercatorIndexCount &&
           mercatorIndexCount % 3U == 0U && sphereIndexCount > 0U &&
           sphereIndexCount <= limits::MaximumSphereIndexCount && sphereIndexCount % 3U == 0U &&
           borderIndexCount <= limits::MaximumBorderIndexCount && borderIndexCount % 2U == 0U &&
           validLandCounts;
}

template <typename Value>
void copyLittleEndianArray(std::vector<Value> &destination, const unsigned char *source,
                           std::size_t &cursor) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    const std::size_t byteCount = destination.size() * sizeof(Value);
    if (byteCount > 0U) {
        std::memcpy(destination.data(), source + cursor, byteCount);
    }
    cursor += byteCount;
#else
    for (Value &value : destination) {
        if constexpr (std::is_same_v<Value, float>) {
            const std::uint32_t bits = readUint32(source + cursor);
            std::memcpy(&value, &bits, sizeof(value));
        } else {
            value = readUint32(source + cursor);
        }
        cursor += sizeof(Value);
    }
#endif
}

bool verticesAreValid(const MapMesh &mesh) noexcept {
    for (std::size_t offset = 0U; offset + 2U < mesh.vertices.size(); offset += 3U) {
        const double longitude = mesh.vertices[offset];
        const double latitude = mesh.vertices[offset + 1U];
        const double mercatorY = mesh.vertices[offset + 2U];
        if (!std::isfinite(longitude) || !std::isfinite(latitude) || !std::isfinite(mercatorY) ||
            std::abs(longitude) > limits::MaximumAbsoluteLongitude ||
            std::abs(latitude) > limits::MaximumAbsoluteLatitude ||
            std::abs(mercatorY) > limits::MaximumAbsoluteMercatorY) {

            return false;
        }
    }
    return true;
}

bool indicesAreValid(const std::vector<std::uint32_t> &indices, std::size_t vertexCount) noexcept {
    for (const std::uint32_t index : indices) {
        if (static_cast<std::size_t>(index) >= vertexCount) {
            return false;
        }
    }
    return true;
}

} // namespace

MapAssetReadResult MapAssetReader::read(QByteArrayView bytes) {
    if (bytes.size() < static_cast<qsizetype>(format::HeaderSize) ||
        bytes.size() > limits::MaximumAssetBytes) {

        return failure(MapAssetError::Size,
                       QStringLiteral("The packaged map has an invalid size."));
    }

    const auto *data = reinterpret_cast<const unsigned char *>(bytes.data());
    if (std::memcmp(data + format::MagicOffset, format::Magic.data(), format::Magic.size()) != 0) {
        return failure(MapAssetError::Format,
                       QStringLiteral("The packaged map has an unsupported signature."));
    }

    const std::uint32_t version = readUint32(data + format::VersionOffset);
    const std::uint32_t headerSize = readUint32(data + format::HeaderSizeOffset);
    const std::uint32_t flags = readUint32(data + format::FlagsOffset);
    if (version != format::Version || headerSize != format::HeaderSize ||
        flags != format::SupportedFlags || readUint32(data + format::ReservedOffset) != 0U) {

        return failure(MapAssetError::Format,
                       QStringLiteral("The packaged map format is not supported."));
    }

    const std::uint64_t vertexCount = readUint64(data + format::VertexCountOffset);
    const std::uint64_t mercatorIndexCount = readUint64(data + format::MercatorIndexCountOffset);
    const std::uint64_t sphereIndexCount = readUint64(data + format::SphereIndexCountOffset);
    const std::uint64_t borderIndexCount = readUint64(data + format::BorderIndexCountOffset);
    const std::uint64_t landCellCount = readUint64(data + format::LandCellCountOffset);
    const std::uint64_t landReferenceCount = readUint64(data + format::LandReferenceCountOffset);
    const std::uint64_t declaredPayloadSize = readUint64(data + format::PayloadSizeOffset);
    const std::uint32_t expectedCrc = readUint32(data + format::PayloadCrcOffset);

    if (!validCounts(vertexCount, mercatorIndexCount, sphereIndexCount, borderIndexCount,
                     landCellCount, landReferenceCount)) {
        return failure(MapAssetError::Limits,
                       QStringLiteral("The packaged map exceeds geometry limits."));
    }
    std::size_t vertexFloatCount = 0U;
    std::size_t vertexBytes = 0U;
    std::size_t mercatorBytes = 0U;
    std::size_t sphereBytes = 0U;
    std::size_t borderBytes = 0U;
    std::size_t landCellValueCount = 0U;
    std::size_t landCellBytes = 0U;
    std::size_t landReferenceBytes = 0U;
    std::size_t expectedPayloadSize = 0U;
    std::size_t expectedFileSize = 0U;
    if (!checkedMultiply(static_cast<std::size_t>(vertexCount), 3U, vertexFloatCount) ||
        !checkedMultiply(vertexFloatCount, sizeof(float), vertexBytes) ||
        !checkedMultiply(static_cast<std::size_t>(mercatorIndexCount), sizeof(std::uint32_t),
                         mercatorBytes) ||
        !checkedMultiply(static_cast<std::size_t>(sphereIndexCount), sizeof(std::uint32_t),
                         sphereBytes) ||
        !checkedMultiply(static_cast<std::size_t>(borderIndexCount), sizeof(std::uint32_t),
                         borderBytes) ||
        !checkedMultiply(static_cast<std::size_t>(landCellCount), 2U, landCellValueCount) ||
        !checkedMultiply(landCellValueCount, sizeof(std::uint32_t), landCellBytes) ||
        !checkedMultiply(static_cast<std::size_t>(landReferenceCount), sizeof(std::uint32_t),
                         landReferenceBytes) ||
        !checkedAdd(vertexBytes, mercatorBytes, expectedPayloadSize) ||
        !checkedAdd(expectedPayloadSize, sphereBytes, expectedPayloadSize) ||
        !checkedAdd(expectedPayloadSize, borderBytes, expectedPayloadSize) ||
        !checkedAdd(expectedPayloadSize, landCellBytes, expectedPayloadSize) ||
        !checkedAdd(expectedPayloadSize, landReferenceBytes, expectedPayloadSize) ||
        expectedPayloadSize > limits::MaximumPayloadBytes ||
        declaredPayloadSize != expectedPayloadSize ||
        !checkedAdd(static_cast<std::size_t>(format::HeaderSize), expectedPayloadSize,
                    expectedFileSize) ||
        expectedFileSize != static_cast<std::size_t>(bytes.size())) {

        return failure(MapAssetError::Format,
                       QStringLiteral("The packaged map sections are inconsistent."));
    }

    const unsigned char *payload = data + format::HeaderSize;
    if (MapChecksum::crc32(payload, expectedPayloadSize) != expectedCrc) {
        return failure(MapAssetError::Integrity,
                       QStringLiteral("The packaged map checksum does not match."));
    }

    try {
        auto mutableMesh = std::make_shared<MapMesh>();
        mutableMesh->vertices.resize(vertexFloatCount);
        mutableMesh->mercatorFillIndices.resize(static_cast<std::size_t>(mercatorIndexCount));
        mutableMesh->sphereFillIndices.resize(static_cast<std::size_t>(sphereIndexCount));
        mutableMesh->borderIndices.resize(static_cast<std::size_t>(borderIndexCount));
        mutableMesh->landCellRanges.resize(static_cast<std::size_t>(landCellCount));
        mutableMesh->landTriangleReferences.resize(static_cast<std::size_t>(landReferenceCount));

        std::size_t cursor = 0U;
        copyLittleEndianArray(mutableMesh->vertices, payload, cursor);
        copyLittleEndianArray(mutableMesh->mercatorFillIndices, payload, cursor);
        copyLittleEndianArray(mutableMesh->sphereFillIndices, payload, cursor);
        copyLittleEndianArray(mutableMesh->borderIndices, payload, cursor);
        for (MapLandCellRange &range : mutableMesh->landCellRanges) {
            range.firstReference = readUint32(payload + cursor);
            cursor += sizeof(std::uint32_t);
            range.referenceCount = readUint32(payload + cursor);
            cursor += sizeof(std::uint32_t);
        }
        copyLittleEndianArray(mutableMesh->landTriangleReferences, payload, cursor);

        if (cursor != expectedPayloadSize || !verticesAreValid(*mutableMesh)) {
            return failure(MapAssetError::Format,
                           QStringLiteral("The packaged map contains invalid vertices."));
        }
        const std::size_t loadedVertexCount = mutableMesh->vertexCount();
        const bool validLandIndex =
            mutableMesh->landCellRanges.empty() || mapLandIndexIsValid(*mutableMesh);
        if (!indicesAreValid(mutableMesh->mercatorFillIndices, loadedVertexCount) ||
            !indicesAreValid(mutableMesh->sphereFillIndices, loadedVertexCount) ||
            !indicesAreValid(mutableMesh->borderIndices, loadedVertexCount) || !validLandIndex) {

            return failure(MapAssetError::Format,
                           QStringLiteral("The packaged map contains invalid indices."));
        }

        return {std::shared_ptr<const MapMesh>(std::move(mutableMesh)), MapAssetError::None, {}};
    } catch (const std::bad_alloc &) {
        return failure(MapAssetError::Allocation,
                       QStringLiteral("There is not enough memory to load the map."));
    } catch (const std::length_error &) {
        return failure(MapAssetError::Limits,
                       QStringLiteral("The packaged map exceeds container limits."));
    }
}

} // namespace lar::map
