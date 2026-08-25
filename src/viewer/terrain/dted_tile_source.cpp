#include "viewer/terrain/dted_tile_source.h"

#include <QDir>
#include <QFileInfo>

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

DtedTileSource::DtedTileSource(QString rootDirectory)
    : m_rootDirectory(QDir::cleanPath(std::move(rootDirectory))) {}

bool DtedTileSource::isAvailable() const {
    return !m_rootDirectory.isEmpty() && QFileInfo(m_rootDirectory).isDir();
}

QString DtedTileSource::pathFor(const DtedCellKey &key) const {
    const QChar longitudeHemisphere =
        key.longitudeDegrees < 0 ? QLatin1Char('w') : QLatin1Char('e');
    const QChar latitudeHemisphere = key.latitudeDegrees < 0 ? QLatin1Char('s') : QLatin1Char('n');
    const QString longitudeDirectory =
        QStringLiteral("%1%2")
            .arg(longitudeHemisphere)
            .arg(std::abs(key.longitudeDegrees), 3, 10, QLatin1Char('0'));
    const QString latitudeFile = QStringLiteral("%1%2.dt0")
                                     .arg(latitudeHemisphere)
                                     .arg(std::abs(key.latitudeDegrees), 2, 10, QLatin1Char('0'));
    return QDir(m_rootDirectory).filePath(longitudeDirectory + QLatin1Char('/') + latitudeFile);
}

DtedCellReadResult DtedTileSource::load(const DtedCellKey &key) const {
    if (key.longitudeDegrees < -180 || key.longitudeDegrees > 179 || key.latitudeDegrees < -90 ||
        key.latitudeDegrees > 89) {
        return {nullptr, QStringLiteral("DTED0 cell key is outside the WGS84 tile range.")};
    }
    const QString path = pathFor(key);
    DtedCellReadResult result = DtedCellReader::readFile(path);
    if (result.succeeded() && !(result.cell->key == key)) {
        return {nullptr,
                QStringLiteral("DTED0 tile '%1' declares a different cell origin.").arg(path)};
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
