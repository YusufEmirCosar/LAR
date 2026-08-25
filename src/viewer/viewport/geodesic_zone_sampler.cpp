
#include "viewer/viewport/geodesic_zone_sampler.h"

#include "viewer/viewport/lar_zone_mesh_limits.h"

#include <QPointF>

#include <algorithm>
#include <cmath>

namespace {

constexpr double Pi = LarGeodesicGeometry::Pi;
constexpr double TwoPi = LarGeodesicGeometry::TwoPi;
constexpr double ToDegrees = 180.0 / Pi;

double poleDistance(double centerLatitude, double poleLatitude) noexcept {
    return std::abs(poleLatitude - centerLatitude) * LarGeodesicGeometry::EarthRadiusMeters;
}

bool isFullCircle(double span) noexcept {
    return span >= TwoPi - LarZoneMeshLimits::FullCircleTolerance;
}

void appendUnique(std::vector<double> &samples, double value) {
    const auto duplicate = [value](double existing) {
        return std::abs(existing - value) <= LarZoneMeshLimits::SampleTolerance;
    };
    if (std::none_of(samples.begin(), samples.end(), duplicate)) {
        samples.push_back(value);
    }
}

std::vector<double> bearingSamples(double start, double span, int segments) {
    const int safeSegments = std::max(1, segments);
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(safeSegments + 3));
    for (int index = 0; index <= safeSegments; ++index) {
        samples.push_back(start +
                          span * static_cast<double>(index) / static_cast<double>(safeSegments));
    }
    const double end = start + span;
    for (const double poleBearing : {0.0, Pi}) {
        const double turns = std::ceil((start - poleBearing) / TwoPi);
        const double candidate = poleBearing + turns * TwoPi;
        if (candidate > start + LarZoneMeshLimits::SampleTolerance &&
            candidate < end - LarZoneMeshLimits::SampleTolerance) {
            appendUnique(samples, candidate);
        }
    }
    std::sort(samples.begin(), samples.end());
    return samples;
}

std::vector<double> radialSamples(const LarZoneDefinition &zone, int segments) {
    const int safeSegments = std::max(1, segments);
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(safeSegments + 3));
    for (int index = 0; index <= safeSegments; ++index) {
        samples.push_back(zone.innerRadiusMeters +
                          (zone.outerRadiusMeters - zone.innerRadiusMeters) *
                              static_cast<double>(index) / static_cast<double>(safeSegments));
    }
    for (const double distance : {poleDistance(zone.center.latitude, Pi * 0.5),
                                  poleDistance(zone.center.latitude, -Pi * 0.5)}) {
        if (distance > zone.innerRadiusMeters + LarZoneMeshLimits::SampleTolerance &&
            distance < zone.outerRadiusMeters - LarZoneMeshLimits::SampleTolerance) {
            appendUnique(samples, distance);
        }
    }
    std::sort(samples.begin(), samples.end());
    return samples;
}

int angularSegmentCount(const LarZoneDefinition &zone, const lar::map::MapCamera &camera, int width,
                        int height) {
    const int minimum = std::max(8, static_cast<int>(std::ceil(64.0 * zone.spanRadians / TwoPi)));
    if (zone.outerRadiusMeters <= 0.0 || width <= 0 || height <= 0) {
        return minimum;
    }
    QPointF centerScreen;
    bool centerVisible = false;
    if (!camera.projectGeoToScreen(zone.center.longitude * ToDegrees,
                                   zone.center.latitude * ToDegrees, width, height, centerScreen,
                                   centerVisible)) {
        return minimum;
    }
    double radiusPixels = 0.0;
    for (const double bearing : {0.0, Pi * 0.5, Pi, Pi * 1.5}) {
        const auto point =
            LarGeodesicGeometry::destination(zone.center, zone.outerRadiusMeters, bearing);
        QPointF pointScreen;
        bool pointVisible = false;
        if (camera.projectGeoToScreen(point.longitude * ToDegrees, point.latitude * ToDegrees,
                                      width, height, pointScreen, pointVisible)) {
            radiusPixels = std::max(radiusPixels, std::hypot(pointScreen.x() - centerScreen.x(),
                                                             pointScreen.y() - centerScreen.y()));
        }
    }
    if (!std::isfinite(radiusPixels) || radiusPixels <= LarZoneMeshLimits::CurveErrorPixels) {
        return minimum;
    }
    const double cosineArgument =
        std::clamp(1.0 - LarZoneMeshLimits::CurveErrorPixels / radiusPixels, -1.0, 1.0);
    const double step = 2.0 * std::acos(cosineArgument);
    if (!std::isfinite(step) || step <= 1.0e-6) {
        return LarZoneMeshLimits::MaximumAngularSegmentCount;
    }
    return std::clamp(static_cast<int>(std::ceil(zone.spanRadians / step)), minimum,
                      LarZoneMeshLimits::MaximumAngularSegmentCount);
}

int radialSegmentCount(const LarZoneDefinition &zone) noexcept {
    const double thickness =
        (zone.outerRadiusMeters - zone.innerRadiusMeters) / LarGeodesicGeometry::EarthRadiusMeters;
    return std::clamp(
        static_cast<int>(std::ceil(thickness / LarZoneMeshLimits::MaximumRadialStepRadians)), 1,
        LarZoneMeshLimits::MaximumRadialSegmentCount);
}

double stableStartBearing(const LarZoneDefinition &zone) noexcept {
    if (!isFullCircle(zone.spanRadians))
        return zone.startBearingRadians;
    const double north = poleDistance(zone.center.latitude, Pi * 0.5);
    const double south = poleDistance(zone.center.latitude, -Pi * 0.5);
    const bool crossesNorth = zone.outerRadiusMeters > north + LarZoneMeshLimits::SampleTolerance;
    const bool crossesSouth = zone.outerRadiusMeters > south + LarZoneMeshLimits::SampleTolerance;
    if (crossesNorth && (!crossesSouth || north <= south))
        return 0.0;
    if (crossesSouth)
        return Pi;
    return zone.startBearingRadians;
}

} // namespace

GeodesicZoneSampleGrid GeodesicZoneSampler::sample(const LarZoneDefinition &zone,
                                                   const lar::map::MapCamera &camera,
                                                   int viewportWidth, int viewportHeight) const {
    int radialSegments = radialSegmentCount(zone);
    int angularSegments = angularSegmentCount(zone, camera, viewportWidth, viewportHeight);
    const int minimumAngular =
        std::max(8, static_cast<int>(std::ceil(64.0 * zone.spanRadians / TwoPi)));
    if (angularSegments * radialSegments > LarZoneMeshLimits::MaximumFillCellsPerZone) {
        angularSegments =
            std::max(minimumAngular, LarZoneMeshLimits::MaximumFillCellsPerZone / radialSegments);
    }
    if (angularSegments * radialSegments > LarZoneMeshLimits::MaximumFillCellsPerZone) {
        radialSegments = std::max(1, LarZoneMeshLimits::MaximumFillCellsPerZone / angularSegments);
    }

    GeodesicZoneSampleGrid result;
    result.fullCircle = isFullCircle(zone.spanRadians);
    result.startBearingRadians = stableStartBearing(zone);
    result.bearings = bearingSamples(result.startBearingRadians, zone.spanRadians, angularSegments);
    result.radii = radialSamples(zone, radialSegments);
    const double seamPole = std::abs(std::remainder(result.startBearingRadians, TwoPi)) <
                                    LarZoneMeshLimits::FullCircleTolerance
                                ? Pi * 0.5
                                : -Pi * 0.5;
    result.seamPoleDistanceMeters = poleDistance(zone.center.latitude, seamPole);
    result.points.reserve(result.rowCount() * result.columnCount());
    for (double radius : result.radii) {
        for (double bearing : result.bearings) {
            result.points.push_back(LarGeodesicGeometry::destination(zone.center, radius, bearing));
        }
    }
    return result;
}
