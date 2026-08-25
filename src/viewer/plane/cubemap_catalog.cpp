
#include "viewer/plane/cubemap_catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMap>
#include <QRegularExpression>
#include <QTransform>

#include <utility>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace {

constexpr qsizetype MaximumSkyboxCount = 256;
constexpr qint64 MaximumFaceBytes = 64 * 1024 * 1024;
constexpr int MaximumFaceSize = 4096;
// OpenGL order: +X, -X, +Y, -Y, +Z, -Z. The source cubemaps use an
// outward-looking convention, so their left image is OpenGL +X.
constexpr std::array<const char *, 6> FaceSuffixes = {"lf", "rt", "up", "dn", "ft", "bk"};

void setError(QString *destination, const QString &message) {
    if (destination != nullptr) {
        *destination = message;
    }
}

int faceIndex(const QString &suffix) {
    for (std::size_t index = 0; index < FaceSuffixes.size(); ++index) {
        if (suffix.compare(QLatin1String(FaceSuffixes[index]), Qt::CaseInsensitive) == 0) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

bool validFaceFile(const QString &path, QSize *expectedSize) {
    const QFileInfo file(path);
    if (!file.isFile() || !file.isReadable() || file.isSymLink() || file.size() <= 0 ||
        file.size() > MaximumFaceBytes) {
        return false;
    }
    QImageReader reader(path);
    reader.setAutoTransform(false);
    const QSize size = reader.size();
    if (!size.isValid() || size.width() != size.height() || size.width() > MaximumFaceSize) {
        return false;
    }
    if (!expectedSize->isValid()) {
        *expectedSize = size;
        return true;
    }
    return size == *expectedSize;
}

} // namespace

CubemapCatalog::CubemapCatalog(QString directory) {
    const QDir cubemapDirectory(std::move(directory));
    const QRegularExpression pattern(QStringLiteral(R"(^(.+)_(rt|lf|up|dn|ft|bk)\.png$)"),
                                     QRegularExpression::CaseInsensitiveOption);
    const QFileInfoList files = cubemapDirectory.entryInfoList(
        {QStringLiteral("*.png")}, QDir::Files | QDir::Readable | QDir::NoSymLinks, QDir::Name);
    QMap<QString, std::array<QString, 6>> groupedPaths;
    for (const QFileInfo &file : files) {
        const auto match = pattern.match(file.fileName());
        if (!match.hasMatch()) {
            continue;
        }
        const QString key = match.captured(1);
        if (!groupedPaths.contains(key) && groupedPaths.size() >= MaximumSkyboxCount) {
            break;
        }
        const int index = faceIndex(match.captured(2));
        if (index < 0) {
            continue;
        }
        groupedPaths[key][static_cast<std::size_t>(index)] = file.absoluteFilePath();
    }

    for (auto group = groupedPaths.cbegin(); group != groupedPaths.cend(); ++group) {
        QSize faceSize;
        bool valid = true;
        for (const QString &path : group.value()) {
            if (path.isEmpty() || !validFaceFile(path, &faceSize)) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            ++m_rejectedSetCount;
            continue;
        }
        const int displayNumber = static_cast<int>(m_entries.size()) + 1;
        m_entries.append(
            {group.value(), QStringLiteral("Sky %1").arg(displayNumber, 2, 10, QLatin1Char('0'))});
    }
}

QString CubemapCatalog::displayName(int index) const {
    return index >= 0 && index < m_entries.size() ? m_entries[index].name : QString{};
}

bool CubemapCatalog::load(int index, CubemapFaces *faces, QString *errorMessage) const {
    if (faces == nullptr || index < 0 || index >= m_entries.size()) {
        setError(errorMessage, QStringLiteral("The requested skybox is unavailable."));
        return false;
    }
    CubemapFaces result;
    QSize expectedSize;
    for (std::size_t face = 0; face < result.images.size(); ++face) {
        const QString &path = m_entries[index].facePaths[face];
        const QFileInfo fileInfo(path);
        if (!fileInfo.isFile() || !fileInfo.isReadable() || fileInfo.isSymLink() ||
            fileInfo.size() <= 0 || fileInfo.size() > MaximumFaceBytes) {
            setError(errorMessage,
                     QStringLiteral("A cubemap face no longer passes file safety limits."));
            return false;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > MaximumFaceBytes) {
            setError(errorMessage, QStringLiteral("A cubemap face could not be opened safely."));
            return false;
        }
        const QFileInfo openedPath(path);
        if (!openedPath.isFile() || !openedPath.isReadable() || openedPath.isSymLink() ||
            openedPath.size() != file.size()) {
            setError(errorMessage,
                     QStringLiteral("A cubemap face changed while it was being opened."));
            return false;
        }
#ifdef Q_OS_UNIX
        struct stat pathStatus{};
        struct stat openedStatus{};
        const QByteArray nativePath = QFile::encodeName(openedPath.absoluteFilePath());
        if (::lstat(nativePath.constData(), &pathStatus) != 0 ||
            ::fstat(file.handle(), &openedStatus) != 0 || !S_ISREG(pathStatus.st_mode) ||
            !S_ISREG(openedStatus.st_mode) || pathStatus.st_dev != openedStatus.st_dev ||
            pathStatus.st_ino != openedStatus.st_ino) {
            setError(errorMessage,
                     QStringLiteral("A cubemap face changed while it was being opened."));
            return false;
        }
#endif
        QImageReader reader(&file);
        reader.setAutoTransform(false);
        const QSize declaredSize = reader.size();
        if (!declaredSize.isValid() || declaredSize.width() != declaredSize.height() ||
            declaredSize.width() > MaximumFaceSize ||
            (expectedSize.isValid() && declaredSize != expectedSize)) {
            setError(errorMessage,
                     QStringLiteral("A cubemap face exceeds the bounded square dimensions."));
            return false;
        }
        const QImage decoded = reader.read();
        if (decoded.isNull() || decoded.width() != decoded.height() ||
            decoded.width() > MaximumFaceSize || decoded.size() != declaredSize ||
            (expectedSize.isValid() && decoded.size() != expectedSize)) {
            setError(
                errorMessage,
                QStringLiteral("A cubemap face is missing, invalid, or has a mismatched size."));
            return false;
        }
        expectedSize = decoded.size();
        QTransform orientation;
        if (face == 2U) {
            orientation.rotate(90.0);
        } else if (face == 3U) {
            orientation.rotate(-90.0);
        }
        result.images[face] =
            decoded.convertToFormat(QImage::Format_RGBA8888).transformed(orientation);
    }
    result.displayName = m_entries[index].name;
    *faces = std::move(result);
    return true;
}
