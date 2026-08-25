#pragma once

/**
 * @file earth_camera_policy.h
 * @brief Pure tracking, fit, zoom-anchor, and scale policy for Earth views.
 */

#include "domain/state.h"
#include "viewer/map/map_camera.h"
#include "viewer/viewport/viewport_camera_controller.h"

#include <QBitArray>
#include <QPointF>

#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

/** Pure Earth viewport tracking, fit-input, and navigation-scale policy. */
class EarthCameraPolicy final {
  public:
    using Coordinate = std::pair<double, double>; // latitude/longitude radians

    static bool hasFields(const QBitArray &available, std::initializer_list<int> fields) noexcept;
    static bool fieldsChanged(const Plane &previousPlane, const Target &previousTarget,
                              const QBitArray &previousFields, bool hadPreviousState,
                              const Plane &nextPlane, const Target &nextTarget,
                              const QBitArray &nextFields,
                              std::initializer_list<int> fields) noexcept;
    static std::optional<Coordinate> trackedCoordinate(const ViewportCameraState &cameraState,
                                                       bool hasScene) noexcept;
    static double bearingDegrees(const ViewportCameraState &cameraState) noexcept;
    static std::vector<Coordinate> fitCoordinates(const Plane &plane, const Target &target,
                                                  const QBitArray &available, bool hasScene);
    static QPointF zoomAnchor(const ViewportCameraState &cameraState, bool hasScene, int width,
                              int height, const QPointF &cursor) noexcept;
    static double navigationMetersPerPixel(const lar::map::MapCamera &camera, int width,
                                           int height) noexcept;
};
