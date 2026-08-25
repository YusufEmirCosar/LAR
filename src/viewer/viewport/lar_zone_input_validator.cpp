
#include "viewer/viewport/lar_zone_input_validator.h"

#include "domain/statefield.h"
#include "viewer/viewport/lar_zone_mesh_limits.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace {

bool hasFields(const QBitArray &available, std::initializer_list<int> fields) noexcept {
    return std::all_of(fields.begin(), fields.end(), [&available](int field) {
        return field >= 0 && field < available.size() && available.testBit(field);
    });
}

bool validCoordinate(double latitude, double longitude) noexcept {
    constexpr double HalfPi = LarGeodesicGeometry::Pi * 0.5;
    return std::isfinite(latitude) && std::isfinite(longitude) && latitude >= -HalfPi &&
           latitude <= HalfPi && longitude >= -LarGeodesicGeometry::Pi &&
           longitude <= LarGeodesicGeometry::Pi;
}

bool validRadius(double radius) noexcept {
    return std::isfinite(radius) && radius >= 0.0 &&
           radius <= LarZoneMeshLimits::MaximumRadiusMeters;
}

} // namespace

LarZoneValidationResult
LarZoneInputValidator::validate(const Target &target,
                                const QBitArray &availableFields) const noexcept {
    LarZoneValidationResult result;
    if (hasFields(availableFields, {StateField::IrPos0, StateField::IrPos1, StateField::IrR})) {
        if (validCoordinate(target.ir_pos[0], target.ir_pos[1]) && validRadius(target.ir_r)) {
            result.inRange = LarZoneDefinition{LarZoneKind::InRange,
                                               {target.ir_pos[0], target.ir_pos[1]},
                                               0.0,
                                               target.ir_r,
                                               0.0,
                                               LarGeodesicGeometry::TwoPi};
        } else {
            result.inputRejected = true;
        }
    }

    if (!hasFields(availableFields, {StateField::IzPos0, StateField::IzPos1, StateField::IzTheta1,
                                     StateField::IzTheta2, StateField::IzR1, StateField::IzR2})) {
        return result;
    }
    const double span =
        LarGeodesicGeometry::positiveAngularSpan(target.iz_theta1, target.iz_theta2);
    if (!validCoordinate(target.iz_pos[0], target.iz_pos[1]) || !std::isfinite(target.iz_theta1) ||
        !std::isfinite(target.iz_theta2) || !validRadius(target.iz_r1) ||
        !validRadius(target.iz_r2) || target.iz_r1 > target.iz_r2 || !std::isfinite(span) ||
        span <= 0.0 || span > LarGeodesicGeometry::TwoPi) {
        result.inputRejected = true;
        return result;
    }
    result.inZone = LarZoneDefinition{LarZoneKind::InZone,
                                      {target.iz_pos[0], target.iz_pos[1]},
                                      target.iz_r1,
                                      target.iz_r2,
                                      std::remainder(target.iz_theta1, LarGeodesicGeometry::TwoPi),
                                      span};
    return result;
}
