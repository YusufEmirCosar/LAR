#pragma once

/**
 * @file lar_zone_mesh.h
 * @brief CPU mesh and draw-range representation for LAR zones.
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include <QMetaType>
#include <QPointF>

/** Identifies how the three stored vertex components are interpreted. */
enum class LarZoneCoordinateSpace : quint8 {
    GeographicDegrees,
    MercatorCameraRelative,
    SphereCameraRelative,
};

/** @brief Contiguous range inside the shared zone index buffer. */
struct LarZoneDrawRange final {
    std::size_t firstIndex = 0U;
    std::size_t indexCount = 0U;
};

/** @brief Dynamic vertices/indices for zone fills and outlines. */
struct LarZoneMesh final {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    LarZoneDrawRange inRangeFill;
    LarZoneDrawRange inZoneFill;
    LarZoneDrawRange inRangeLines;
    LarZoneDrawRange inZoneLines;
    LarZoneCoordinateSpace coordinateSpace = LarZoneCoordinateSpace::GeographicDegrees;
    /** Longitude/Mercator-Y or longitude/latitude origin subtracted before float conversion. */
    QPointF coordinateOrigin;
    bool mercatorGeometryClipped = false;
    bool inputRejected = false;

    /**
     * @brief Reports whether the zone mesh has no renderable geometry.
     *
     * @return True when vertices or indices are empty.
     */
    [[nodiscard]] bool empty() const noexcept {
        return vertices.empty() || indices.empty();
    }

    void clear() noexcept {
        vertices.clear();
        indices.clear();
        inRangeFill = {};
        inZoneFill = {};
        inRangeLines = {};
        inZoneLines = {};
        coordinateSpace = LarZoneCoordinateSpace::GeographicDegrees;
        coordinateOrigin = {};
        mercatorGeometryClipped = false;
        inputRejected = false;
    }
};

Q_DECLARE_METATYPE(LarZoneMesh)
