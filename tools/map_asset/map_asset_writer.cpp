
#include "map_asset_writer.h"

#include "viewer/map/map_asset_format.h"
#include "viewer/map/map_asset_limits.h"
#include "viewer/map/map_checksum.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtEndian>

#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

namespace lar::map::tool {
namespace {

void setError(QString *destination, const QString &message) {
    if (destination != nullptr) {
        *destination = message;
    }
}

void appendUint32(QByteArray &destination, std::uint32_t value) {
    const std::uint32_t encoded = qToLittleEndian(value);
    destination.append(reinterpret_cast<const char *>(&encoded),
                       static_cast<qsizetype>(sizeof(encoded)));
}

void appendUint64(QByteArray &destination, std::uint64_t value) {
    const std::uint64_t encoded = qToLittleEndian(value);
    destination.append(reinterpret_cast<const char *>(&encoded),
                       static_cast<qsizetype>(sizeof(encoded)));
}

template <typename Value>
void appendArray(QByteArray &destination, const std::vector<Value> &values) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    if (!values.empty()) {
        destination.append(reinterpret_cast<const char *>(values.data()),
                           static_cast<qsizetype>(values.size() * sizeof(Value)));
    }
#else
    for (const Value value : values) {
        if constexpr (std::is_same_v<Value, float>) {
            std::uint32_t bits = 0U;
            std::memcpy(&bits, &value, sizeof(bits));
            appendUint32(destination, bits);
        } else {
            appendUint32(destination, value);
        }
    }
#endif
}

bool validate(const MapMesh &mesh) {
    if (mesh.empty() || mesh.vertices.size() % 3U != 0U ||
        mesh.vertexCount() > limits::MaximumVertexCount || mesh.mercatorFillIndices.empty() ||
        mesh.mercatorFillIndices.size() > limits::MaximumMercatorIndexCount ||
        mesh.mercatorFillIndices.size() % 3U != 0U || mesh.sphereFillIndices.empty() ||
        mesh.sphereFillIndices.size() > limits::MaximumSphereIndexCount ||
        mesh.sphereFillIndices.size() % 3U != 0U ||
        mesh.borderIndices.size() > limits::MaximumBorderIndexCount ||
        mesh.borderIndices.size() % 2U != 0U || mesh.byteSize() > limits::MaximumPayloadBytes ||
        mesh.byteSize() > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
        return false;
    }

    for (std::size_t offset = 0U; offset + 2U < mesh.vertices.size(); offset += 3U) {
        if (!std::isfinite(mesh.vertices[offset]) || !std::isfinite(mesh.vertices[offset + 1U]) ||
            !std::isfinite(mesh.vertices[offset + 2U]) ||
            std::abs(mesh.vertices[offset]) > limits::MaximumAbsoluteLongitude ||
            std::abs(mesh.vertices[offset + 1U]) > limits::MaximumAbsoluteLatitude ||
            std::abs(mesh.vertices[offset + 2U]) > limits::MaximumAbsoluteMercatorY) {
            return false;
        }
    }

    const auto indicesAreValid =
        [vertexCount = mesh.vertexCount()](const std::vector<std::uint32_t> &indices) {
            for (const std::uint32_t index : indices) {
                if (static_cast<std::size_t>(index) >= vertexCount) {
                    return false;
                }
            }
            return true;
        };
    return indicesAreValid(mesh.mercatorFillIndices) && indicesAreValid(mesh.sphereFillIndices) &&
           indicesAreValid(mesh.borderIndices);
}

bool writeSingleFile(const QString &path, const QByteArray &bytes, QString *errorMessage) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setError(errorMessage, file.errorString());
        return false;
    }
    return true;
}

struct PreviousFile final {
    bool existed = false;
    QByteArray bytes;
};

bool capturePreviousFile(const QString &path, PreviousFile *previous, QString *errorMessage) {
    const QFileInfo info(path);
    if (!info.exists()) {
        *previous = {};
        return true;
    }
    if (!info.isFile() || info.isSymLink() || info.size() < 0 ||
        info.size() > limits::MaximumAssetBytes) {
        setError(errorMessage,
                 QStringLiteral("The previous map asset cannot be backed up safely."));
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, file.errorString());
        return false;
    }
    const QByteArray bytes = file.read(limits::MaximumAssetBytes + 1);
    if (bytes.size() != info.size()) {
        setError(errorMessage,
                 QStringLiteral("The previous map asset changed while it was backed up."));
        return false;
    }
    *previous = {true, bytes};
    return true;
}

bool restorePreviousFile(const QString &path, const PreviousFile &previous, QString *errorMessage) {
    if (previous.existed) {
        return writeSingleFile(path, previous.bytes, errorMessage);
    }
    if (!QFileInfo::exists(path) || QFile::remove(path)) {
        return true;
    }
    setError(errorMessage, QStringLiteral("The newly committed map asset could not be removed."));
    return false;
}

bool writeFilePair(const QString &assetPath, const QByteArray &assetBytes,
                   const QString &manifestPath, const QByteArray &manifestBytes,
                   QString *errorMessage) {
    if (QFileInfo(assetPath).absoluteFilePath() == QFileInfo(manifestPath).absoluteFilePath()) {
        setError(errorMessage, QStringLiteral("Map asset and manifest paths must be different."));
        return false;
    }
    PreviousFile previousAsset;
    if (!capturePreviousFile(assetPath, &previousAsset, errorMessage)) {
        return false;
    }

    QSaveFile assetFile(assetPath);
    QSaveFile manifestFile(manifestPath);
    if (!assetFile.open(QIODevice::WriteOnly) || assetFile.write(assetBytes) != assetBytes.size()) {
        setError(errorMessage, assetFile.errorString());
        return false;
    }
    if (!manifestFile.open(QIODevice::WriteOnly) ||
        manifestFile.write(manifestBytes) != manifestBytes.size()) {
        setError(errorMessage, manifestFile.errorString());
        return false;
    }
    if (!assetFile.commit()) {
        setError(errorMessage, assetFile.errorString());
        return false;
    }
    if (manifestFile.commit()) {
        return true;
    }

    const QString commitError = manifestFile.errorString();
    QString rollbackError;
    if (!restorePreviousFile(assetPath, previousAsset, &rollbackError)) {
        setError(errorMessage,
                 QStringLiteral("Manifest commit failed (%1) and asset rollback failed (%2).")
                     .arg(commitError, rollbackError));
    } else {
        setError(errorMessage,
                 QStringLiteral("Manifest commit failed; the previous asset was restored: %1")
                     .arg(commitError));
    }
    return false;
}

} // namespace

bool MapAssetWriter::write(const QString &assetPath, const QString &manifestPath,
                           const MapMesh &mesh, QString *errorMessage) {
    if (!validate(mesh)) {
        setError(errorMessage, QStringLiteral("The map mesh is invalid or exceeds asset limits."));
        return false;
    }

    QByteArray payload;
    payload.reserve(static_cast<qsizetype>(mesh.byteSize()));
    appendArray(payload, mesh.vertices);
    appendArray(payload, mesh.mercatorFillIndices);
    appendArray(payload, mesh.sphereFillIndices);
    appendArray(payload, mesh.borderIndices);

    QByteArray asset;
    asset.reserve(static_cast<qsizetype>(format::HeaderSize) + payload.size());
    asset.append(format::Magic.data(), format::Magic.size());
    appendUint32(asset, format::Version);
    appendUint32(asset, format::HeaderSize);
    appendUint32(asset, format::SupportedFlags);
    appendUint64(asset, mesh.vertexCount());
    appendUint64(asset, mesh.mercatorFillIndices.size());
    appendUint64(asset, mesh.sphereFillIndices.size());
    appendUint64(asset, mesh.borderIndices.size());
    appendUint64(asset, mesh.byteSize());
    appendUint32(asset,
                 MapChecksum::crc32(reinterpret_cast<const unsigned char *>(payload.constData()),
                                    static_cast<std::size_t>(payload.size())));
    appendUint32(asset, 0U);
    if (asset.size() != static_cast<qsizetype>(format::HeaderSize)) {
        setError(errorMessage, QStringLiteral("The map asset header could not be generated."));
        return false;
    }
    asset.append(payload);

    const QJsonObject manifest{
        {QStringLiteral("formatVersion"), static_cast<int>(format::Version)},
        {QStringLiteral("bytes"), static_cast<qint64>(asset.size())},
        {QStringLiteral("sha256"),
         QString::fromLatin1(QCryptographicHash::hash(asset, QCryptographicHash::Sha256).toHex())}};
    const QByteArray manifestBytes = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    return writeFilePair(assetPath, asset, manifestPath, manifestBytes, errorMessage);
}

} // namespace lar::map::tool
