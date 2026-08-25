
#include "viewer/viewport/earth_camera_policy.h"

#include "domain/statefield.h"
#include "viewer/lar_geodesic_geometry.h"
#include "viewer/map/map_projection.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

bool EarthCameraPolicy::hasFields(const QBitArray &available,
                                  std::initializer_list<int> fields) noexcept {
    return std::all_of(fields.begin(), fields.end(), [&available](int field) {
        return field >= 0 && field < available.size() && available.testBit(field);
    });
}

bool EarthCameraPolicy::fieldsChanged(const Plane &previousPlane, const Target &previousTarget,
                                      const QBitArray &previousFields, bool hadPreviousState,
                                      const Plane &nextPlane, const Target &nextTarget,
                                      const QBitArray &nextFields,
                                      std::initializer_list<int> fields) noexcept {
    for (int field : fields) {
        const bool wasAvailable = hadPreviousState && field >= 0 && field < previousFields.size() &&
                                  previousFields.testBit(field);
        const bool isAvailable =
            field >= 0 && field < nextFields.size() && nextFields.testBit(field);
        if (wasAvailable != isAvailable)
            return true;
        if (isAvailable && StateField::value(previousPlane, previousTarget, field) !=
                               StateField::value(nextPlane, nextTarget, field)) {
            return true;
        }
    }
    return false;
}

std::optional<EarthCameraPolicy::Coordinate>
EarthCameraPolicy::trackedCoordinate(const ViewportCameraState &cameraState,
                                     bool hasScene) noexcept {
    constexpr double Pi = LarGeodesicGeometry::Pi;
    const double latitude = cameraState.anchorRadians[0];
    const double longitude = cameraState.anchorRadians[1];
    if (!hasScene || !cameraState.trackingActive || !cameraState.hasAnchor ||
        !std::isfinite(latitude) || !std::isfinite(longitude) || latitude < -Pi * 0.5 ||
        latitude > Pi * 0.5 || longitude < -Pi || longitude > Pi) {
        return std::nullopt;
    }
    return Coordinate{latitude, longitude};
}

double EarthCameraPolicy::bearingDegrees(const ViewportCameraState &cameraState) noexcept {
    return cameraState.bearingRadians * 180.0 / LarGeodesicGeometry::Pi;
}

std::vector<EarthCameraPolicy::Coordinate>
EarthCameraPolicy::fitCoordinates(const Plane &plane, const Target &target,
                                  const QBitArray &available, bool hasScene) {
    std::vector<Coordinate> coordinates;
    if (!hasScene)
        return coordinates;
    const auto append = [&coordinates](double latitude, double longitude) {
        coordinates.emplace_back(latitude, longitude);
    };
    if (hasFields(available, {StateField::Location0, StateField::Location1})) {
        append(plane.location[0], plane.location[1]);
    }
    if (hasFields(available, {StateField::IzPos0, StateField::IzPos1})) {
        append(target.iz_pos[0], target.iz_pos[1]);
    }
    if (hasFields(available, {StateField::IrPos0, StateField::IrPos1})) {
        append(target.ir_pos[0], target.ir_pos[1]);
    }
    if (hasFields(available, {StateField::IrPos0, StateField::IrPos1, StateField::IrR})) {
        for (const auto &point :
             LarGeodesicGeometry::arc({target.ir_pos[0], target.ir_pos[1]}, target.ir_r, 0.0,
                                      LarGeodesicGeometry::TwoPi, 64)) {
            append(point.latitude, point.longitude);
        }
    }
    if (hasFields(available, {StateField::IzPos0, StateField::IzPos1, StateField::IzTheta1,
                              StateField::IzTheta2, StateField::IzR2})) {
        const double span =
            LarGeodesicGeometry::positiveAngularSpan(target.iz_theta1, target.iz_theta2);
        for (const auto &point : LarGeodesicGeometry::arc(
                 {target.iz_pos[0], target.iz_pos[1]}, target.iz_r2, target.iz_theta1, span, 64)) {
            append(point.latitude, point.longitude);
        }
    }
    return coordinates;
}

QPointF EarthCameraPolicy::zoomAnchor(const ViewportCameraState &cameraState, bool hasScene,
                                      int width, int height, const QPointF &cursor) noexcept {
    return hasScene && cameraState.trackingActive && cameraState.hasAnchor
               ? QPointF(width * 0.5, height * 0.5)
               : cursor;
}

double EarthCameraPolicy::navigationMetersPerPixel(const lar::map::MapCamera &camera, int width,
                                                   int height) noexcept {
    if (width <= 1 || height <= 1)
        return 0.0;
    constexpr double HalfPixel = 0.5;
    const QPointF center(width * 0.5, height * 0.5);
    double longitudeA = 0.0;
    double latitudeA = 0.0;
    double longitudeB = 0.0;
    double latitudeB = 0.0;
    if (camera.presentation() == lar::map::MapPresentation::Sphere) {
        if (!camera.sphereCoordinateAt({center.x() - HalfPixel, center.y()}, width, height,
                                       longitudeA, latitudeA) ||
            !camera.sphereCoordinateAt({center.x() + HalfPixel, center.y()}, width, height,
                                       longitudeB, latitudeB)) {
            return 0.0;
        }
    } else {
        const QPointF geographicA = lar::map::MapProjection::unproject(
            camera.screenToMercator({center.x() - HalfPixel, center.y()}, width, height));
        const QPointF geographicB = lar::map::MapProjection::unproject(
            camera.screenToMercator({center.x() + HalfPixel, center.y()}, width, height));
        longitudeA = geographicA.x();
        latitudeA = geographicA.y();
        longitudeB = geographicB.x();
        latitudeB = geographicB.y();
    }
    if (!std::isfinite(longitudeA) || !std::isfinite(latitudeA) || !std::isfinite(longitudeB) ||
        !std::isfinite(latitudeB)) {
        return 0.0;
    }

    const double latitudeARadians = qDegreesToRadians(latitudeA);
    const double latitudeBRadians = qDegreesToRadians(latitudeB);
    const double deltaLatitude = latitudeBRadians - latitudeARadians;
    const double deltaLongitude =
        qDegreesToRadians(lar::map::MapProjection::wrapLongitude(longitudeB - longitudeA));
    const double sineLatitude = std::sin(deltaLatitude * 0.5);
    const double sineLongitude = std::sin(deltaLongitude * 0.5);
    const double haversine = std::clamp(
        sineLatitude * sineLatitude +
            std::cos(latitudeARadians) * std::cos(latitudeBRadians) * sineLongitude * sineLongitude,
        0.0, 1.0);
    const double distance =
        LarGeodesicGeometry::EarthRadiusMeters * 2.0 *
        std::atan2(std::sqrt(haversine), std::sqrt(std::max(0.0, 1.0 - haversine)));
    return std::isfinite(distance) && distance > 0.0 ? distance : 0.0;
}
