#include "viewer/plane/glb_resource_reader.h"

#include "viewer/plane/glb_resource_limits.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QUrl>
#include <QtEndian>

#include <cstring>
#include <limits>
#include <utility>

namespace lar::gltf {
namespace {

constexpr quint32 GlbMagic = 0x46546C67U;
constexpr quint32 JsonChunk = 0x4E4F534AU;
constexpr quint32 BinaryChunk = 0x004E4942U;

quint32 littleU32(const char *source) noexcept {
    quint32 encoded = 0U;
    std::memcpy(&encoded, source, sizeof(encoded));
    return qFromLittleEndian(encoded);
}

bool integerValue(const QJsonObject &object, const QString &key, int *value) {
    if (value == nullptr || !object.value(key).isDouble())
        return false;
    const qint64 integer = object.value(key).toInteger(std::numeric_limits<qint64>::min());
    if (integer < 0 || integer > std::numeric_limits<int>::max())
        return false;
    *value = static_cast<int>(integer);
    return true;
}

bool decodeDataUri(const QString &uri, qint64 maximumBytes, QByteArray *result) {
    if (result == nullptr || maximumBytes < 0 ||
        !uri.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
        return false;
    }
    const qsizetype separator = uri.indexOf(QLatin1Char(','));
    if (separator <= 5)
        return false;
    const QByteArray metadata = uri.left(separator).toUtf8().toLower();
    const QByteArray payload = uri.mid(separator + 1).toUtf8();
    if (payload.size() > maximumBytes * 4 / 3 + 16)
        return false;

    QByteArray decoded;
    if (metadata.split(';').contains(QByteArrayLiteral("base64"))) {
        const auto decoding =
            QByteArray::fromBase64Encoding(payload, QByteArray::AbortOnBase64DecodingErrors);
        if (decoding.decodingStatus != QByteArray::Base64DecodingStatus::Ok)
            return false;
        decoded = decoding.decoded;
    } else {
        decoded = QByteArray::fromPercentEncoding(payload);
    }
    if (decoded.size() > maximumBytes)
        return false;
    *result = std::move(decoded);
    return true;
}

bool containedRegularFile(const QString &uri, const QString &sourceDirectory,
                          QString *canonicalPath) {
    if (canonicalPath == nullptr || uri.isEmpty() || sourceDirectory.isEmpty())
        return false;
    const QUrl parsed = QUrl::fromEncoded(uri.toUtf8(), QUrl::StrictMode);
    if (!parsed.isValid() || !parsed.isRelative() || !parsed.scheme().isEmpty() ||
        !parsed.authority().isEmpty() || parsed.hasQuery() || parsed.hasFragment()) {
        return false;
    }

    QString relative = QDir::fromNativeSeparators(parsed.path(QUrl::FullyDecoded));
    if (relative.isEmpty() || relative.contains(QChar::Null) || QDir::isAbsolutePath(relative))
        return false;
    relative = QDir::cleanPath(relative);
    if (relative == QStringLiteral("..") || relative.startsWith(QStringLiteral("../")) ||
        relative == QStringLiteral(".")) {
        return false;
    }

    const QString base = QFileInfo(sourceDirectory).canonicalFilePath();
    if (base.isEmpty() || !QFileInfo(base).isDir())
        return false;

    QString componentPath = base;
    const QStringList components = relative.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &component : components) {
        componentPath = QDir(componentPath).filePath(component);
        if (QFileInfo(componentPath).isSymLink())
            return false;
    }

    const QFileInfo candidate(QDir(base).filePath(relative));
    const QString canonicalCandidate = candidate.canonicalFilePath();
    if (canonicalCandidate.isEmpty() || !candidate.isFile() || candidate.isSymLink())
        return false;
    const QString contained = QDir(base).relativeFilePath(canonicalCandidate);
    if (QDir::isAbsolutePath(contained) || contained == QStringLiteral("..") ||
        contained.startsWith(QStringLiteral("../"))) {
        return false;
    }
    *canonicalPath = canonicalCandidate;
    return true;
}

} // namespace

bool GlbResourceReader::isGlb(const QByteArray &bytes, const QString &suffix) noexcept {
    return suffix.compare(QStringLiteral("glb"), Qt::CaseInsensitive) == 0 ||
           (bytes.size() >= qsizetype(sizeof(quint32)) && littleU32(bytes.constData()) == GlbMagic);
}

bool GlbResourceReader::splitGlb(const QByteArray &bytes, GlbContainer *container) {
    if (container == nullptr || bytes.size() < 20 || littleU32(bytes.constData()) != GlbMagic ||
        littleU32(bytes.constData() + 4) != 2U ||
        littleU32(bytes.constData() + 8) != static_cast<quint32>(bytes.size())) {
        return false;
    }

    GlbContainer parsed;
    bool foundJson = false;
    qint64 offset = 12;
    while (offset + 8 <= bytes.size()) {
        const quint32 chunkLength = littleU32(bytes.constData() + static_cast<qsizetype>(offset));
        const quint32 chunkType = littleU32(bytes.constData() + static_cast<qsizetype>(offset + 4));
        offset += 8;
        if (chunkLength == 0U || offset + static_cast<qint64>(chunkLength) > bytes.size())
            return false;
        const QByteArray chunk =
            bytes.mid(static_cast<qsizetype>(offset), static_cast<qsizetype>(chunkLength));
        if (chunkType == JsonChunk && !foundJson) {
            parsed.json = chunk;
            foundJson = true;
        } else if (chunkType == BinaryChunk && !parsed.hasBinary) {
            parsed.binary = chunk;
            parsed.hasBinary = true;
        }
        offset += static_cast<qint64>(chunkLength);
    }
    if (offset != bytes.size() || !foundJson)
        return false;
    while (!parsed.json.isEmpty() && parsed.json.back() == '\0')
        parsed.json.chop(1);
    if (parsed.json.isEmpty())
        return false;
    *container = std::move(parsed);
    return true;
}

bool GlbResourceReader::readUri(const QString &uri, const QString &sourceDirectory,
                                qint64 maximumBytes, QByteArray *result) {
    QByteArray bytes;
    if (decodeDataUri(uri, maximumBytes, &bytes)) {
        *result = std::move(bytes);
        return true;
    }
    if (result == nullptr || maximumBytes < 0 ||
        uri.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
        return false;
    }

    QString path;
    if (!containedRegularFile(uri, sourceDirectory, &path))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > maximumBytes)
        return false;
    bytes = file.readAll();
    if (bytes.size() != file.size())
        return false;
    // Re-check after opening to narrow the path-replacement window.
    QString verifiedPath;
    if (!containedRegularFile(uri, sourceDirectory, &verifiedPath) || verifiedPath != path)
        return false;
    *result = std::move(bytes);
    return true;
}

bool GlbResourceReader::resolveBuffers(const QJsonArray &bufferTable,
                                       const QString &sourceDirectory, const QByteArray &glbBinary,
                                       bool hasGlbBinary, std::vector<QByteArray> *binaries) {
    if (binaries == nullptr || bufferTable.isEmpty() ||
        bufferTable.size() > limits::MaximumBufferCount) {
        return false;
    }

    qint64 aggregateBytes = 0;
    for (const QJsonValue &candidate : bufferTable) {
        int byteLength = -1;
        if (!candidate.isObject() ||
            !integerValue(candidate.toObject(), QStringLiteral("byteLength"), &byteLength) ||
            byteLength < 0 || byteLength > limits::MaximumResourceBytes ||
            aggregateBytes > limits::MaximumAggregateBufferBytes - byteLength) {
            return false;
        }
        aggregateBytes += byteLength;
    }

    std::vector<QByteArray> resolved;
    resolved.reserve(static_cast<std::size_t>(bufferTable.size()));
    for (qsizetype index = 0; index < bufferTable.size(); ++index) {
        const QJsonObject bufferObject = bufferTable[index].toObject();
        int byteLength = -1;
        if (!integerValue(bufferObject, QStringLiteral("byteLength"), &byteLength))
            return false;
        const QJsonValue uriValue = bufferObject.value(QStringLiteral("uri"));
        QByteArray binary;
        if (uriValue.isString()) {
            if (!readUri(uriValue.toString(), sourceDirectory, limits::MaximumResourceBytes,
                         &binary)) {
                return false;
            }
        } else if (!uriValue.isUndefined() || !hasGlbBinary || index != 0) {
            return false;
        } else {
            binary = glbBinary;
        }
        if (binary.size() < byteLength)
            return false;
        binary.truncate(byteLength);
        resolved.push_back(std::move(binary));
    }
    *binaries = std::move(resolved);
    return true;
}

} // namespace lar::gltf
