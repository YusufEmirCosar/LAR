
#include "viewer/map/packaged_map_asset_source.h"

#include "viewer/map/map_asset_format.h"
#include "viewer/map/map_asset_limits.h"
#include "viewer/map/map_asset_reader.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace lar::map {
namespace {

constexpr qint64 MaximumManifestBytes = 4096;

MapAssetReadResult failure(MapAssetError error, const QString &message) {
    return {nullptr, error, message};
}

bool isDirectChild(const QString &canonicalDirectory, const QFileInfo &fileInfo) {
    if (canonicalDirectory.isEmpty() || !fileInfo.exists() || !fileInfo.isFile() ||
        fileInfo.isSymLink()) {

        return false;
    }
    return QFileInfo(fileInfo.canonicalPath()).canonicalFilePath() == canonicalDirectory;
}

bool openTrustedDirectChild(const QString &canonicalDirectory, const QString &path, QFile *file) {
    const QFileInfo before(path);
    if (!isDirectChild(canonicalDirectory, before)) {
        return false;
    }
    file->setFileName(before.absoluteFilePath());
    if (!file->open(QIODevice::ReadOnly)) {
        return false;
    }
    const QFileInfo after(path);
    if (!isDirectChild(canonicalDirectory, after)) {
        return false;
    }
#ifdef Q_OS_UNIX
    struct stat pathStatus{};
    struct stat openedStatus{};
    const QByteArray nativePath = QFile::encodeName(after.absoluteFilePath());
    if (::lstat(nativePath.constData(), &pathStatus) != 0 ||
        ::fstat(file->handle(), &openedStatus) != 0 || !S_ISREG(pathStatus.st_mode) ||
        !S_ISREG(openedStatus.st_mode) || pathStatus.st_dev != openedStatus.st_dev ||
        pathStatus.st_ino != openedStatus.st_ino) {
        return false;
    }
#endif
    return true;
}

bool readBounded(QFile *file, qint64 maximumBytes, QByteArray *bytes) {
    const qint64 expectedSize = file->size();
    if (expectedSize <= 0 || expectedSize > maximumBytes) {
        return false;
    }
    *bytes = file->read(maximumBytes + 1);
    return bytes->size() == expectedSize;
}

} // namespace

PackagedMapAssetSource::PackagedMapAssetSource(QString packageDirectory)
    : m_packageDirectory(QDir::cleanPath(std::move(packageDirectory))) {}

QString PackagedMapAssetSource::assetPath() const {
    return QDir(m_packageDirectory).filePath(QStringLiteral("lar_world_map.larmap"));
}

QString PackagedMapAssetSource::manifestPath() const {
    return QDir(m_packageDirectory).filePath(QStringLiteral("lar_world_map.manifest.json"));
}

MapAssetReadResult PackagedMapAssetSource::load() const {
    const QFileInfo directoryInfo(m_packageDirectory);
    const QString canonicalDirectory = directoryInfo.canonicalFilePath();
    if (!directoryInfo.exists() || !directoryInfo.isDir() || directoryInfo.isSymLink() ||
        canonicalDirectory.isEmpty()) {

        return failure(MapAssetError::Io,
                       QStringLiteral("The packaged map directory is unavailable."));
    }

    QFile assetFile;
    QFile manifestFile;
    if (!openTrustedDirectChild(canonicalDirectory, assetPath(), &assetFile) ||
        !openTrustedDirectChild(canonicalDirectory, manifestPath(), &manifestFile)) {
        return failure(MapAssetError::Integrity,
                       QStringLiteral("The packaged map location is not trusted."));
    }
    const qint64 assetSize = assetFile.size();
    if (assetSize < static_cast<qint64>(format::HeaderSize) ||
        assetSize > limits::MaximumAssetBytes) {
        return failure(MapAssetError::Size,
                       QStringLiteral("The packaged map has an invalid size."));
    }
    if (manifestFile.size() <= 0 || manifestFile.size() > MaximumManifestBytes) {
        return failure(MapAssetError::Integrity,
                       QStringLiteral("The packaged map manifest has an invalid size."));
    }

    QByteArray manifestBytes;
    if (!readBounded(&manifestFile, MaximumManifestBytes, &manifestBytes)) {
        return failure(MapAssetError::Io,
                       QStringLiteral("The packaged map manifest could not be read safely."));
    }
    QJsonParseError parseError;
    const QJsonDocument manifestDocument = QJsonDocument::fromJson(manifestBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !manifestDocument.isObject()) {
        return failure(MapAssetError::Integrity,
                       QStringLiteral("The packaged map manifest is invalid."));
    }
    const QJsonObject manifest = manifestDocument.object();
    const QJsonValue sizeValue = manifest.value(QStringLiteral("bytes"));
    const QJsonValue versionValue = manifest.value(QStringLiteral("formatVersion"));
    const QJsonValue shaValue = manifest.value(QStringLiteral("sha256"));
    if (!sizeValue.isDouble() || !versionValue.isDouble() || !shaValue.isString()) {
        return failure(MapAssetError::Integrity,
                       QStringLiteral("The packaged map manifest is invalid."));
    }
    const qint64 expectedSize = sizeValue.toInteger(-1);
    const int expectedVersion = versionValue.toInt(-1);
    const QByteArray expectedSha = shaValue.toString().toLatin1().toLower();
    if (expectedSize != assetSize || expectedVersion != static_cast<int>(format::Version) ||
        expectedSha.size() != 64) {

        return failure(MapAssetError::Integrity,
                       QStringLiteral("The packaged map manifest does not match."));
    }

    QByteArray assetBytes;
    if (!readBounded(&assetFile, limits::MaximumAssetBytes, &assetBytes) ||
        assetBytes.size() != expectedSize) {
        return failure(MapAssetError::Io,
                       QStringLiteral("The packaged map could not be read completely."));
    }
    const QByteArray actualSha =
        QCryptographicHash::hash(assetBytes, QCryptographicHash::Sha256).toHex();
    if (actualSha != expectedSha) {
        return failure(MapAssetError::Integrity,
                       QStringLiteral("The packaged map integrity check failed."));
    }

    return MapAssetReader::read(QByteArrayView(assetBytes));
}

} // namespace lar::map
