
#include "viewer/plane/glb_texture_reader.h"
#include "viewer/plane/glb_resource_limits.h"
#include "viewer/plane/glb_resource_reader.h"

#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonObject>
#include <QUrl>

#include <limits>
#include <utility>
#include <vector>

namespace {

bool integerValue(const QJsonObject &object, const QString &key, int *value, int fallback = -1) {
    const QJsonValue candidate = object.value(key);
    if (candidate.isUndefined()) {
        *value = fallback;
        return fallback >= 0;
    }
    if (!candidate.isDouble()) {
        return false;
    }
    const qint64 integer = candidate.toInteger(std::numeric_limits<qint64>::min());
    if (integer < 0 || integer > std::numeric_limits<int>::max()) {
        return false;
    }
    *value = static_cast<int>(integer);
    return true;
}

bool optionalIndex(const QJsonObject &object, const QString &key, int *value) {
    if (object.value(key).isUndefined()) {
        *value = -1;
        return true;
    }
    return integerValue(object, key, value);
}

bool bufferViewData(const QJsonArray &bufferViews, const std::vector<QByteArray> &binaries,
                    int viewIndex, QByteArray *result) {
    if (result == nullptr || viewIndex < 0 || viewIndex >= bufferViews.size()) {
        return false;
    }
    const QJsonObject view = bufferViews[viewIndex].toObject();
    int bufferIndex = -1;
    int byteOffset = 0;
    int byteLength = -1;
    if (!integerValue(view, QStringLiteral("buffer"), &bufferIndex) || bufferIndex < 0 ||
        bufferIndex >= static_cast<int>(binaries.size()) ||
        !integerValue(view, QStringLiteral("byteOffset"), &byteOffset, 0) ||
        !integerValue(view, QStringLiteral("byteLength"), &byteLength) || byteLength <= 0) {
        return false;
    }
    const QByteArray &binary = binaries[static_cast<std::size_t>(bufferIndex)];
    const qint64 end = static_cast<qint64>(byteOffset) + byteLength;
    if (end > binary.size()) {
        return false;
    }
    *result = binary.mid(byteOffset, byteLength);
    return result->size() == byteLength;
}

bool textureWrap(int encoded, PlaneTextureWrap *result) {
    switch (encoded) {
    case 33071:
        *result = PlaneTextureWrap::ClampToEdge;
        return true;
    case 33648:
        *result = PlaneTextureWrap::MirroredRepeat;
        return true;
    case 10497:
        *result = PlaneTextureWrap::Repeat;
        return true;
    default:
        return false;
    }
}

bool textureMinFilter(int encoded, PlaneTextureMinFilter *result) {
    switch (encoded) {
    case 9728:
        *result = PlaneTextureMinFilter::Nearest;
        return true;
    case 9729:
        *result = PlaneTextureMinFilter::Linear;
        return true;
    case 9984:
        *result = PlaneTextureMinFilter::NearestMipmapNearest;
        return true;
    case 9985:
        *result = PlaneTextureMinFilter::LinearMipmapNearest;
        return true;
    case 9986:
        *result = PlaneTextureMinFilter::NearestMipmapLinear;
        return true;
    case 9987:
        *result = PlaneTextureMinFilter::LinearMipmapLinear;
        return true;
    default:
        return false;
    }
}

bool textureMagFilter(int encoded, PlaneTextureMagFilter *result) {
    switch (encoded) {
    case 9728:
        *result = PlaneTextureMagFilter::Nearest;
        return true;
    case 9729:
        *result = PlaneTextureMagFilter::Linear;
        return true;
    default:
        return false;
    }
}

bool imagePayload(const QJsonObject &imageObject, const QJsonArray &bufferViews,
                  const std::vector<QByteArray> &binaries, const QString &sourceDirectory,
                  QByteArray *encoded) {
    if (encoded == nullptr) {
        return false;
    }
    const QJsonValue viewValue = imageObject.value(QStringLiteral("bufferView"));
    const QJsonValue uriValue = imageObject.value(QStringLiteral("uri"));
    if (!viewValue.isUndefined()) {
        int viewIndex = -1;
        if (!uriValue.isUndefined() ||
            !integerValue(imageObject, QStringLiteral("bufferView"), &viewIndex)) {
            return false;
        }
        return bufferViewData(bufferViews, binaries, viewIndex, encoded);
    }
    return uriValue.isString() &&
           lar::gltf::GlbResourceReader::readUri(uriValue.toString(), sourceDirectory,
                                                 lar::gltf::limits::MaximumResourceBytes, encoded);
}

QByteArray imageFormat(const QJsonObject &imageObject) {
    const QString mimeType = imageObject.value(QStringLiteral("mimeType")).toString();
    if (mimeType == QStringLiteral("image/png")) {
        return QByteArrayLiteral("png");
    }
    if (mimeType == QStringLiteral("image/jpeg")) {
        return QByteArrayLiteral("jpeg");
    }
    if (!mimeType.isEmpty())
        return {};

    const QString uri = imageObject.value(QStringLiteral("uri")).toString();
    if (uri.startsWith(QStringLiteral("data:image/png;"), Qt::CaseInsensitive))
        return QByteArrayLiteral("png");
    if (uri.startsWith(QStringLiteral("data:image/jpeg;"), Qt::CaseInsensitive))
        return QByteArrayLiteral("jpeg");
    const QString suffix =
        QFileInfo(QUrl::fromEncoded(uri.toUtf8(), QUrl::StrictMode).path()).suffix().toLower();
    if (suffix == QStringLiteral("png"))
        return QByteArrayLiteral("png");
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg"))
        return QByteArrayLiteral("jpeg");
    return {};
}

bool decodeImages(const QJsonArray &bufferViews, const QJsonArray &imageTable,
                  const std::vector<QByteArray> &binaries, const QString &sourceDirectory,
                  std::vector<QImage> *images) {
    images->reserve(static_cast<std::size_t>(imageTable.size()));
    qint64 totalEncodedBytes = 0;
    qint64 totalDecodedBytes = 0;
    for (const QJsonValue &candidate : imageTable) {
        const QJsonObject imageObject = candidate.toObject();
        if (!candidate.isObject()) {
            return false;
        }
        QByteArray encoded;
        if (!imagePayload(imageObject, bufferViews, binaries, sourceDirectory, &encoded) ||
            encoded.isEmpty() || encoded.size() > lar::gltf::limits::MaximumResourceBytes ||
            totalEncodedBytes >
                lar::gltf::limits::MaximumAggregateEncodedImageBytes - encoded.size()) {
            return false;
        }
        totalEncodedBytes += encoded.size();
        QBuffer buffer(&encoded);
        if (!buffer.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QByteArray format = imageFormat(imageObject);
        if (format.isEmpty())
            return false;
        QImageReader reader(&buffer, format);
        reader.setAutoDetectImageFormat(false);
        reader.setDecideFormatFromContent(false);
        const QSize size = reader.size();
        if (!size.isValid() || size.width() > lar::gltf::limits::MaximumTextureDimension ||
            size.height() > lar::gltf::limits::MaximumTextureDimension) {
            return false;
        }
        const qint64 pixels = static_cast<qint64>(size.width()) * size.height();
        constexpr qint64 RgbaBytesPerPixel = 4;
        const qint64 decodedBytes = pixels * RgbaBytesPerPixel;
        if (pixels <= 0 || decodedBytes <= 0 ||
            totalDecodedBytes >
                lar::gltf::limits::MaximumAggregateDecodedImageBytes - decodedBytes) {
            return false;
        }
        QImage decoded = reader.read();
        if (decoded.isNull() || decoded.size() != size) {
            return false;
        }
        decoded = decoded.convertToFormat(QImage::Format_RGBA8888);
        if (decoded.isNull()) {
            return false;
        }
        totalDecodedBytes += decodedBytes;
        images->push_back(std::move(decoded));
    }
    return true;
}

} // namespace

bool GlbTextureReader::load(const QJsonArray &bufferViews, const QJsonArray &imageTable,
                            const QJsonArray &textureTable, const QJsonArray &samplers,
                            const QByteArray &binary, PlaneModelMesh *mesh) {
    return load(bufferViews, imageTable, textureTable, samplers, std::vector<QByteArray>{binary},
                {}, mesh);
}

bool GlbTextureReader::load(const QJsonArray &bufferViews, const QJsonArray &imageTable,
                            const QJsonArray &textureTable, const QJsonArray &samplers,
                            const std::vector<QByteArray> &binaries, const QString &sourceDirectory,
                            PlaneModelMesh *mesh) {
    if (mesh == nullptr || imageTable.size() > lar::gltf::limits::MaximumTextureCount ||
        textureTable.size() > lar::gltf::limits::MaximumTextureCount ||
        samplers.size() > lar::gltf::limits::MaximumTextureCount) {
        return false;
    }
    std::vector<QImage> images;
    if (!decodeImages(bufferViews, imageTable, binaries, sourceDirectory, &images)) {
        return false;
    }

    std::vector<PlaneModelTexture> textures;
    textures.reserve(static_cast<std::size_t>(textureTable.size()));
    qint64 totalTextureUploadBytes = 0;
    for (const QJsonValue &candidate : textureTable) {
        const QJsonObject textureObject = candidate.toObject();
        int sourceIndex = -1;
        int samplerIndex = -1;
        if (!candidate.isObject() ||
            !integerValue(textureObject, QStringLiteral("source"), &sourceIndex) ||
            sourceIndex >= static_cast<int>(images.size()) ||
            !optionalIndex(textureObject, QStringLiteral("sampler"), &samplerIndex) ||
            samplerIndex >= samplers.size()) {
            return false;
        }

        const qint64 imageBytes = images[static_cast<std::size_t>(sourceIndex)].sizeInBytes();
        if (imageBytes <= 0 ||
            totalTextureUploadBytes >
                lar::gltf::limits::MaximumAggregateDecodedImageBytes - imageBytes) {
            return false;
        }
        totalTextureUploadBytes += imageBytes;

        PlaneModelTexture texture;
        texture.image = images[static_cast<std::size_t>(sourceIndex)];
        if (samplerIndex >= 0) {
            const QJsonObject sampler = samplers[samplerIndex].toObject();
            int wrapS = 10497;
            int wrapT = 10497;
            int minFilter = 9987;
            int magFilter = 9729;
            if (!integerValue(sampler, QStringLiteral("wrapS"), &wrapS, 10497) ||
                !integerValue(sampler, QStringLiteral("wrapT"), &wrapT, 10497) ||
                !integerValue(sampler, QStringLiteral("minFilter"), &minFilter, 9987) ||
                !integerValue(sampler, QStringLiteral("magFilter"), &magFilter, 9729) ||
                !textureWrap(wrapS, &texture.wrapS) || !textureWrap(wrapT, &texture.wrapT) ||
                !textureMinFilter(minFilter, &texture.minFilter) ||
                !textureMagFilter(magFilter, &texture.magFilter)) {
                return false;
            }
        }
        textures.push_back(std::move(texture));
    }
    mesh->textures = std::move(textures);
    return true;
}
