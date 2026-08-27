#include "viewer/terrain/dted_tile_source.h"

#include <QDir>
#include <QFileInfo>

#include <array>
#include <cmath>
#include <utility>

namespace {
constexpr double Pi = 3.14159265358979323846;

double normalizedLongitudeDegrees(double degrees) noexcept {
    double normalized = std::fmod(degrees + 180.0, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized - 180.0;
}
} // namespace

DtedTileSource::DtedTileSource(QString rootDirectory, DtedLevel level)
    : m_rootDirectory(QDir::cleanPath(QDir::fromNativeSeparators(std::move(rootDirectory)))),
      m_canonicalRootDirectory(QFileInfo(m_rootDirectory).canonicalFilePath()), m_level(level) {}

bool DtedTileSource::isAvailable() const {
    return !m_rootDirectory.isEmpty() && !m_canonicalRootDirectory.isEmpty() &&
           QFileInfo(m_rootDirectory).isDir();
}

bool DtedTileSource::containsFile(const QString &path) const {
    if (m_canonicalRootDirectory.isEmpty()) {
        return false;
    }
    const QFileInfo fileInfo(path);
    const QString canonicalFile = fileInfo.canonicalFilePath();
    if (!fileInfo.isFile() || canonicalFile.isEmpty()) {
        return false;
    }

    // QDir paths use forward slashes internally even on Windows, whereas
    // QDir::separator() returns the native backslash there. Comparing a path
    // against a prefix built with the native separator therefore rejected
    // valid Windows tiles. A canonical relative path also gives the boundary
    // check platform-aware drive-letter and case handling without admitting a
    // sibling directory or a symlink target above the selected root.
    const QString relative = QDir::cleanPath(
        QDir::fromNativeSeparators(QDir(m_canonicalRootDirectory).relativeFilePath(canonicalFile)));
    return !relative.isEmpty() && !QDir::isAbsolutePath(relative) &&
           relative != QStringLiteral(".") && relative != QStringLiteral("..") &&
           !relative.startsWith(QStringLiteral("../"));
}

QString DtedTileSource::pathFor(const DtedCellKey &key) const {
    const QChar longitudeHemisphere =
        key.longitudeDegrees < 0 ? QLatin1Char('w') : QLatin1Char('e');
    const QChar latitudeHemisphere = key.latitudeDegrees < 0 ? QLatin1Char('s') : QLatin1Char('n');
    const QString longitudeDirectory =
        QStringLiteral("%1%2")
            .arg(longitudeHemisphere)
            .arg(std::abs(key.longitudeDegrees), 3, 10, QLatin1Char('0'));
    const QString latitudeFile = QStringLiteral("%1%2%3")
                                     .arg(latitudeHemisphere)
                                     .arg(std::abs(key.latitudeDegrees), 2, 10, QLatin1Char('0'))
                                     .arg(dtedFileSuffix(m_level));
    const QString uppercaseLongitudeDirectory = longitudeDirectory.toUpper();
    const QString uppercaseLatitudeFile = latitudeFile.toUpper();
    const std::array<QString, 4> relativeCandidates{
        longitudeDirectory + QLatin1Char('/') + latitudeFile,
        uppercaseLongitudeDirectory + QLatin1Char('/') + uppercaseLatitudeFile,
        longitudeDirectory + QLatin1Char('/') + uppercaseLatitudeFile,
        uppercaseLongitudeDirectory + QLatin1Char('/') + latitudeFile};
    const QDir root(m_rootDirectory);
    for (const QString &relative : relativeCandidates) {
        const QString candidate = root.filePath(relative);
        if (containsFile(candidate)) {
            return candidate;
        }
    }
    return root.filePath(relativeCandidates.front());
}

DtedCellReadResult DtedTileSource::load(const DtedCellKey &key) const {
    if (key.longitudeDegrees < -180 || key.longitudeDegrees > 179 || key.latitudeDegrees < -90 ||
        key.latitudeDegrees > 89) {
        return {nullptr, QStringLiteral("%1 cell key is outside the WGS84 tile range.")
                             .arg(dtedLevelDisplayName(m_level))};
    }
    const QString path = pathFor(key);
    if (QFileInfo(path).isFile() && !containsFile(path)) {
        return {nullptr, QStringLiteral("%1 tile '%2' resolves outside the selected root.")
                             .arg(dtedLevelDisplayName(m_level), path)};
    }
    DtedCellReadResult result = DtedCellReader::readFile(path, m_level);
    if (result.succeeded() && !(result.cell->key == key)) {
        return {nullptr, QStringLiteral("%1 tile '%2' declares a different cell origin.")
                             .arg(dtedLevelDisplayName(m_level), path)};
    }
    return result;
}

std::optional<DtedCellKey> DtedTileSource::keyForRadians(double latitudeRadians,
                                                         double longitudeRadians) noexcept {
    if (!std::isfinite(latitudeRadians) || !std::isfinite(longitudeRadians) ||
        latitudeRadians < -Pi * 0.5 || latitudeRadians > Pi * 0.5) {
        return std::nullopt;
    }
    const double latitudeDegrees = latitudeRadians * 180.0 / Pi;
    const double clampedLatitude = std::clamp(
        latitudeDegrees, -90.0, std::nextafter(90.0, -std::numeric_limits<double>::infinity()));
    const double longitudeDegrees = normalizedLongitudeDegrees(longitudeRadians * 180.0 / Pi);
    return DtedCellKey{static_cast<int>(std::floor(longitudeDegrees)),
                       static_cast<int>(std::floor(clampedLatitude))};
}
