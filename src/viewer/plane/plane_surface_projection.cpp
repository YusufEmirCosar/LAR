
#include "viewer/plane/plane_surface_projection.h"

#include "domain/statefield.h"
#include "viewer/grid_geometry_builder.h"
#include "viewer/lar_projection.h"
#include "viewer/viewport/lar_zone_input_validator.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <optional>

namespace {

constexpr double Pi = LarProjection::Pi;
constexpr double MinimumExtentMeters = 2000.0;
constexpr double SurfaceMargin = 1.2;
constexpr double AltitudeExtentFactor = 50.0;
constexpr double GridCoverageFraction = 0.75;
constexpr float MinimumSurfaceHalfExtentSceneUnits = 64.0F;
constexpr double MinimumTargetHalfWidthMeters = 3.0;
constexpr double MaximumTargetHalfWidthMeters = 25.0;
constexpr float TargetRadiusFraction = 0.005F;

double saturatedProduct(double value, double factor) noexcept {
    const double magnitude = std::abs(value);
    if (magnitude > std::numeric_limits<double>::max() / factor) {
        return std::numeric_limits<double>::max();
    }
    return magnitude * factor;
}

bool hasFields(const QBitArray &fields, std::initializer_list<int> required) noexcept {
    return std::all_of(required.begin(), required.end(), [&fields](int field) {
        return field >= 0 && field < fields.size() && fields.testBit(field);
    });
}

bool validPosition(double latitude, double longitude) noexcept {
    return std::isfinite(latitude) && std::isfinite(longitude) && latitude >= -Pi * 0.5 &&
           latitude <= Pi * 0.5 && longitude >= -Pi && longitude <= Pi;
}

std::optional<QPointF> localMeters(const GeoCoordinateRadians &coordinate,
                                   const Plane &plane) noexcept {
    if (!validPosition(coordinate.latitude, coordinate.longitude)) {
        return std::nullopt;
    }
    const double position[3]{coordinate.latitude, coordinate.longitude, 0.0};
    const QPointF eastNorth = LarProjection::geographicToPlaneWorld(position, plane.location, 0.0,
                                                                    plane.location[0], true);
    if (!std::isfinite(eastNorth.x()) || !std::isfinite(eastNorth.y())) {
        return std::nullopt;
    }
    return eastNorth;
}

std::optional<QPointF> aircraftGroundMeters(const Plane &plane,
                                            const GeoCoordinateRadians &origin) noexcept {
    if (!validPosition(origin.latitude, origin.longitude)) {
        return std::nullopt;
    }
    const double position[3]{plane.location[0], plane.location[1], 0.0};
    const double originPosition[3]{origin.latitude, origin.longitude, 0.0};
    const QPointF eastNorth =
        LarProjection::geographicToPlaneWorld(position, originPosition, 0.0, origin.latitude, true);
    if (!std::isfinite(eastNorth.x()) || !std::isfinite(eastNorth.y())) {
        return std::nullopt;
    }
    return eastNorth;
}

void includeExtent(const QPointF &centerMeters, double radiusMeters, double *extentMeters) {
    *extentMeters = std::max({*extentMeters, std::abs(centerMeters.x()) + radiusMeters,
                              std::abs(centerMeters.y()) + radiusMeters});
}

void applySurfaceLayout(double contentExtentMeters, const std::optional<double> &altitudeMeters,
                        PlaneSurfaceState *result) noexcept {
    if (altitudeMeters) {
        const double surfaceHeight = -*altitudeMeters / result->metersPerSceneUnit;
        result->surfaceHeight = static_cast<float>(
            std::clamp(surfaceHeight, -static_cast<double>(PlaneSurfaceMaximumCoordinate),
                       static_cast<double>(PlaneSurfaceMaximumCoordinate)));
    }
    const double altitudeExtentMeters =
        altitudeMeters ? saturatedProduct(*altitudeMeters, AltitudeExtentFactor) : 0.0;
    const double requiredHalfExtentMeters =
        std::max({MinimumExtentMeters, contentExtentMeters * SurfaceMargin, altitudeExtentMeters});
    const double requiredHalfExtent = requiredHalfExtentMeters / result->metersPerSceneUnit;
    result->surfaceHalfExtent =
        std::clamp(static_cast<float>(requiredHalfExtent), MinimumSurfaceHalfExtentSceneUnits,
                   PlaneSurfaceMaximumCoordinate);
    const double targetGridSpacing =
        std::max(result->metersPerSceneUnit * 0.5,
                 requiredHalfExtentMeters /
                     (static_cast<double>(PlaneSurfaceGridHalfLineCount) * GridCoverageFraction));
    const double maximumGridSpacingMeters = static_cast<double>(PlaneSurfaceMaximumCoordinate) *
                                            result->metersPerSceneUnit /
                                            static_cast<double>(PlaneSurfaceGridHalfLineCount);
    const double maximumQuantizedSpacing =
        std::exp2(std::floor(std::log2(maximumGridSpacingMeters)));
    result->gridSpacingMeters =
        GridGeometryBuilder::gridStep(std::min(targetGridSpacing, maximumQuantizedSpacing));
    result->gridSpacingSceneUnits =
        static_cast<float>(result->gridSpacingMeters / result->metersPerSceneUnit);
}

PlaneSurfaceZone projectedZone(const LarZoneDefinition &definition, const QPointF &centerMeters,
                               double metersPerSceneUnit) noexcept {
    PlaneSurfaceZone zone;
    zone.centerXZ = {static_cast<float>(centerMeters.x() / metersPerSceneUnit),
                     static_cast<float>(-centerMeters.y() / metersPerSceneUnit)};
    zone.innerRadius = static_cast<float>(definition.innerRadiusMeters / metersPerSceneUnit);
    zone.outerRadius = static_cast<float>(definition.outerRadiusMeters / metersPerSceneUnit);
    zone.startBearingRadians = static_cast<float>(definition.startBearingRadians);
    zone.spanRadians = static_cast<float>(definition.spanRadians);
    zone.visible = true;
    zone.fullCircle = definition.spanRadians >= LarGeodesicGeometry::TwoPi - 1.0e-9;
    return zone;
}

float targetMarkerScale(const PlaneSurfaceState &state) noexcept {
    const float referenceRadius = state.inZone.visible    ? state.inZone.outerRadius
                                  : state.inRange.visible ? state.inRange.outerRadius
                                                          : 0.0F;
    const float minimumScale =
        static_cast<float>(MinimumTargetHalfWidthMeters / state.metersPerSceneUnit);
    const float maximumScale =
        static_cast<float>(MaximumTargetHalfWidthMeters / state.metersPerSceneUnit);
    if (!std::isfinite(referenceRadius) || referenceRadius <= 0.0F) {
        return minimumScale;
    }
    return std::clamp(referenceRadius * TargetRadiusFraction, minimumScale, maximumScale);
}

} // namespace

PlaneSurfaceState
PlaneSurfaceProjection::project(const LarSceneState &scene, double metersPerSceneUnit,
                                const std::optional<GeoCoordinateRadians> &groundOrigin) noexcept {
    PlaneSurfaceState result;
    result.metersPerSceneUnit = std::isfinite(metersPerSceneUnit) && metersPerSceneUnit > 0.0
                                    ? metersPerSceneUnit
                                    : PlaneAircraftScale::DefaultMetersPerSceneUnit;
    std::optional<double> altitudeMeters;
    if (scene.hasScene && hasFields(scene.availableFields, {StateField::Location2}) &&
        std::isfinite(scene.plane.location[2])) {
        altitudeMeters = scene.plane.location[2];
    }
    applySurfaceLayout(MinimumExtentMeters, altitudeMeters, &result);
    if (!scene.hasScene ||
        !hasFields(scene.availableFields, {StateField::Location0, StateField::Location1}) ||
        !validPosition(scene.plane.location[0], scene.plane.location[1])) {
        return result;
    }
    result.geographicAnchorValid = true;

    const LarZoneValidationResult validation =
        LarZoneInputValidator().validate(scene.target, scene.availableFields);
    std::optional<QPointF> inRangeCenter;
    std::optional<QPointF> inZoneCenter;
    std::optional<QPointF> targetCenter;
    double extentMeters = MinimumExtentMeters;
    if (hasFields(scene.availableFields, {StateField::IzPos0, StateField::IzPos1})) {
        targetCenter = localMeters({scene.target.iz_pos[0], scene.target.iz_pos[1]}, scene.plane);
    }
    if (!targetCenter &&
        hasFields(scene.availableFields, {StateField::IrPos0, StateField::IrPos1})) {
        targetCenter = localMeters({scene.target.ir_pos[0], scene.target.ir_pos[1]}, scene.plane);
    }
    if (targetCenter) {
        includeExtent(*targetCenter, 0.0, &extentMeters);
    }
    if (validation.inRange) {
        inRangeCenter = localMeters(validation.inRange->center, scene.plane);
        if (inRangeCenter) {
            includeExtent(*inRangeCenter, validation.inRange->outerRadiusMeters, &extentMeters);
        }
    }
    if (validation.inZone) {
        inZoneCenter = localMeters(validation.inZone->center, scene.plane);
        if (inZoneCenter) {
            includeExtent(*inZoneCenter, validation.inZone->outerRadiusMeters, &extentMeters);
        }
    }

    applySurfaceLayout(extentMeters, altitudeMeters, &result);

    if (groundOrigin) {
        const std::optional<QPointF> aircraftGround =
            aircraftGroundMeters(scene.plane, *groundOrigin);
        if (aircraftGround) {
            result.groundOriginXZ = {
                static_cast<float>(-aircraftGround->x() / result.metersPerSceneUnit),
                static_cast<float>(aircraftGround->y() / result.metersPerSceneUnit)};
            const float majorPeriod = result.gridSpacingSceneUnits * 5.0F;
            if (std::isfinite(majorPeriod) && majorPeriod > 0.0F) {
                result.gridPhaseXZ = {std::remainder(result.groundOriginXZ.x(), majorPeriod),
                                      std::remainder(result.groundOriginXZ.y(), majorPeriod)};
            }
        }
    }

    if (validation.inRange && inRangeCenter) {
        result.inRange =
            projectedZone(*validation.inRange, *inRangeCenter, result.metersPerSceneUnit);
    }
    if (validation.inZone && inZoneCenter) {
        result.inZone = projectedZone(*validation.inZone, *inZoneCenter, result.metersPerSceneUnit);
    }
    result.targetMarkerScale = targetMarkerScale(result);

    if (targetCenter) {
        result.targetXZ = {static_cast<float>(targetCenter->x() / result.metersPerSceneUnit),
                           static_cast<float>(-targetCenter->y() / result.metersPerSceneUnit)};
        result.targetVisible = true;
    }
    return result;
}
