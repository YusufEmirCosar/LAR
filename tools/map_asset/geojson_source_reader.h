#pragma once

/**
 * @file geojson_source_reader.h
 * @brief Bounded parser for polygonal GeoJSON source maps.
 */

#include "source_map.h"

#include <QByteArray>
#include <QString>

namespace lar::map::tool {

/** @brief Stable failure categories for source-map ingestion. */
enum class SourceMapError {
    None,
    OpenFailed,
    SizeRejected,
    ReadFailed,
    InvalidJson,
    InvalidSchema,
    ResourceLimit,
    InvalidCoordinate,
    UnsupportedGeometry
};

/** @brief Parsed source map or a categorized diagnostic. */
struct SourceMapReadResult final {
    SourceMap map;
    SourceMapError error = SourceMapError::None;
    QString message;

    /**
     * @brief Reports whether parsing produced a valid non-empty source map.
     */
    [[nodiscard]] bool succeeded() const noexcept {
        return error == SourceMapError::None && !map.empty();
    }
};

/** @brief Validates supported GeoJSON and enforces source resource limits. */
class GeoJsonSourceReader final {
  public:
    static SourceMapReadResult read(const QString &path);
    static SourceMapReadResult parse(const QByteArray &bytes);
};

} // namespace lar::map::tool
