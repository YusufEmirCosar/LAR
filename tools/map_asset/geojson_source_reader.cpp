
#include "geojson_source_reader.h"

#include "map_source_limits.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace lar::map::tool {
namespace {
SourceMapReadResult failure(SourceMapError error, const QString &message) {
    return {{}, error, message};
}

bool checkedIncrement(std::size_t &value, std::size_t maximum) {
    if (value >= maximum) {
        return false;
    }
    ++value;
    return true;
}

bool appendRing(const QJsonArray &coordinates, SourceMap &map, SourceRing &destination,
                SourceMapError &error) {
    if (coordinates.size() < 4) {
        error = SourceMapError::InvalidSchema;
        return false;
    }
    if (static_cast<std::size_t>(coordinates.size()) > source_limits::MaximumCoordinatesPerRing ||
        map.coordinateCount >
            source_limits::MaximumCoordinateCount - static_cast<std::size_t>(coordinates.size())) {
        error = SourceMapError::ResourceLimit;
        return false;
    }

    destination.reserve(static_cast<std::size_t>(coordinates.size()));
    for (const QJsonValue &coordinateValue : coordinates) {
        if (!coordinateValue.isArray()) {
            error = SourceMapError::InvalidCoordinate;
            return false;
        }
        const QJsonArray coordinate = coordinateValue.toArray();
        if (coordinate.size() < 2 || !coordinate.at(0).isDouble() || !coordinate.at(1).isDouble()) {
            error = SourceMapError::InvalidCoordinate;
            return false;
        }
        const double longitude = coordinate.at(0).toDouble();
        const double latitude = coordinate.at(1).toDouble();
        if (!std::isfinite(longitude) || !std::isfinite(latitude) || longitude < -180.0 ||
            longitude > 180.0 || latitude < -90.0 || latitude > 90.0) {
            error = SourceMapError::InvalidCoordinate;
            return false;
        }
        destination.push_back({longitude, latitude});
    }
    if (destination.front().longitudeDegrees != destination.back().longitudeDegrees ||
        destination.front().latitudeDegrees != destination.back().latitudeDegrees) {
        error = SourceMapError::InvalidSchema;
        return false;
    }
    map.coordinateCount += destination.size();
    return true;
}

bool appendPolygon(const QJsonArray &coordinates, SourceMap &map, std::size_t &ringCount,
                   SourceMapError &error) {
    if (coordinates.isEmpty() || map.polygons.size() >= source_limits::MaximumPolygonCount) {
        error =
            coordinates.isEmpty() ? SourceMapError::InvalidSchema : SourceMapError::ResourceLimit;
        return false;
    }

    SourcePolygon polygon;
    polygon.holes.reserve(static_cast<std::size_t>(std::max<qsizetype>(coordinates.size() - 1, 0)));
    for (qsizetype index = 0; index < coordinates.size(); ++index) {
        if (!checkedIncrement(ringCount, source_limits::MaximumRingCount)) {
            error = SourceMapError::ResourceLimit;
            return false;
        }
        if (!coordinates.at(index).isArray()) {
            error = SourceMapError::InvalidSchema;
            return false;
        }
        SourceRing ring;
        if (!appendRing(coordinates.at(index).toArray(), map, ring, error)) {
            return false;
        }
        if (index == 0) {
            polygon.exterior = std::move(ring);
        } else {
            polygon.holes.push_back(std::move(ring));
        }
    }
    map.polygons.push_back(std::move(polygon));
    return true;
}

QString messageFor(SourceMapError error) {
    switch (error) {
    case SourceMapError::InvalidSchema:
        return QStringLiteral("The GeoJSON structure is not a supported feature collection.");
    case SourceMapError::ResourceLimit:
        return QStringLiteral("The GeoJSON geometry exceeds the map compiler limits.");
    case SourceMapError::InvalidCoordinate:
        return QStringLiteral("The GeoJSON contains an invalid coordinate.");
    case SourceMapError::UnsupportedGeometry:
        return QStringLiteral("The GeoJSON contains an unsupported geometry type.");
    default:
        return QStringLiteral("The GeoJSON source is invalid.");
    }
}

} // namespace

SourceMapReadResult GeoJsonSourceReader::read(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(SourceMapError::OpenFailed,
                       QStringLiteral("The map source could not be opened."));
    }
    if (file.size() <= 0 ||
        static_cast<std::uint64_t>(file.size()) > source_limits::MaximumSourceBytes) {
        return failure(SourceMapError::SizeRejected,
                       QStringLiteral("The map source file size is outside the allowed range."));
    }
    const qint64 expectedSize = file.size();
    const QByteArray bytes = file.readAll();
    if (bytes.size() != expectedSize) {
        return failure(SourceMapError::ReadFailed,
                       QStringLiteral("The complete map source could not be read."));
    }
    return parse(bytes);
}

SourceMapReadResult GeoJsonSourceReader::parse(const QByteArray &bytes) {
    if (bytes.isEmpty() ||
        static_cast<std::size_t>(bytes.size()) > source_limits::MaximumSourceBytes) {
        return failure(SourceMapError::SizeRejected,
                       QStringLiteral("The map source byte count is outside the allowed range."));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(SourceMapError::InvalidJson,
                       QStringLiteral("The map source is not valid JSON."));
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("type")).toString() != QStringLiteral("FeatureCollection") ||
        !root.value(QStringLiteral("features")).isArray()) {
        return failure(SourceMapError::InvalidSchema, messageFor(SourceMapError::InvalidSchema));
    }
    const QJsonArray features = root.value(QStringLiteral("features")).toArray();
    if (features.isEmpty() ||
        static_cast<std::size_t>(features.size()) > source_limits::MaximumFeatureCount) {
        return failure(SourceMapError::ResourceLimit, messageFor(SourceMapError::ResourceLimit));
    }

    SourceMap map;
    std::size_t ringCount = 0U;
    SourceMapError error = SourceMapError::None;
    for (const QJsonValue &featureValue : features) {
        if (!featureValue.isObject()) {
            return failure(SourceMapError::InvalidSchema,
                           messageFor(SourceMapError::InvalidSchema));
        }
        const QJsonObject feature = featureValue.toObject();
        if (feature.value(QStringLiteral("type")).toString() != QStringLiteral("Feature") ||
            !feature.value(QStringLiteral("geometry")).isObject()) {
            return failure(SourceMapError::InvalidSchema,
                           messageFor(SourceMapError::InvalidSchema));
        }
        const QJsonObject geometry = feature.value(QStringLiteral("geometry")).toObject();
        const QString type = geometry.value(QStringLiteral("type")).toString();
        if (!geometry.value(QStringLiteral("coordinates")).isArray()) {
            return failure(SourceMapError::InvalidSchema,
                           messageFor(SourceMapError::InvalidSchema));
        }
        const QJsonArray coordinates = geometry.value(QStringLiteral("coordinates")).toArray();
        if (type == QStringLiteral("Polygon")) {
            if (!appendPolygon(coordinates, map, ringCount, error)) {
                return failure(error, messageFor(error));
            }
        } else if (type == QStringLiteral("MultiPolygon")) {
            if (coordinates.isEmpty()) {
                return failure(SourceMapError::InvalidSchema,
                               messageFor(SourceMapError::InvalidSchema));
            }
            for (const QJsonValue &polygonValue : coordinates) {
                if (!polygonValue.isArray() ||
                    !appendPolygon(polygonValue.toArray(), map, ringCount, error)) {
                    if (error == SourceMapError::None) {
                        error = SourceMapError::InvalidSchema;
                    }
                    return failure(error, messageFor(error));
                }
            }
        } else {
            return failure(SourceMapError::UnsupportedGeometry,
                           messageFor(SourceMapError::UnsupportedGeometry));
        }
    }

    if (map.empty()) {
        return failure(SourceMapError::InvalidSchema,
                       QStringLiteral("The map source contains no renderable polygons."));
    }
    return {std::move(map), SourceMapError::None, {}};
}

} // namespace lar::map::tool
