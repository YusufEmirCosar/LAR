#pragma once

/**
 * @file lar_geodesic_geometry.h
 * @brief Spherical destination-point and arc tessellation utilities.
 */

#include <vector>

/** @brief Latitude/longitude pair expressed in radians. */
struct GeoCoordinateRadians {
    double latitude = 0.0;
    double longitude = 0.0;
};

/** @brief Builds geodesic rings and sector arcs around a geographic center. */
class LarGeodesicGeometry final {
  public:
    static constexpr double EarthRadiusMeters = 6371008.8;
    static constexpr double Pi = 3.14159265358979323846;
    static constexpr double TwoPi = Pi * 2.0;

    static double wrapLongitude(double longitudeRadians) noexcept;
    static double positiveAngularSpan(double startRadians, double endRadians) noexcept;

    static GeoCoordinateRadians destination(const GeoCoordinateRadians &center,
                                            double distanceMeters, double bearingRadians) noexcept;

    static std::vector<GeoCoordinateRadians> arc(const GeoCoordinateRadians &center,
                                                 double distanceMeters, double startBearingRadians,
                                                 double spanRadians, int segmentCount);
};
