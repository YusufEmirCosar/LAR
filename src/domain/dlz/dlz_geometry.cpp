
#include "domain/dlz/dlz_geometry.h"

#include "domain/dlz/dlz_units.h"

#include <algorithm>
#include <cmath>

namespace dlz {
namespace {

constexpr double Pi = 3.1415926535897932384626433832795;
bool finite(const Vec3 &value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
double dot(const Vec3 &a, const Vec3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
double norm(const Vec3 &value) {
    return std::sqrt(dot(value, value));
}
Vec3 subtract(const Vec3 &a, const Vec3 &b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 forwardVector(const ShooterState &shooter) {
    const double cosTheta = std::cos(shooter.theta);
    return {cosTheta * std::cos(shooter.psi), cosTheta * std::sin(shooter.psi),
            -std::sin(shooter.theta)};
}

bool validCommonInputs(const ShooterState &shooter, const TargetState &target,
                       const Atmosphere &atmosphere, const WeaponModel &weapon, QString *error) {
    const auto fail = [error](const QString &message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    if (!finite(shooter.pos) || !finite(shooter.vel) || !finite(target.pos) ||
        !finite(target.vel)) {
        return fail(QStringLiteral("DLZ state vectors must be finite"));
    }
    if (!std::isfinite(shooter.psi) || !std::isfinite(shooter.theta) ||
        !std::isfinite(shooter.phi) || !std::isfinite(target.psi)) {
        return fail(QStringLiteral("DLZ attitudes must be finite"));
    }
    if (!std::isfinite(shooter.mach) || shooter.mach < 0.0 || !std::isfinite(target.mach) ||
        target.mach < 0.0) {
        return fail(QStringLiteral("DLZ Mach values must be finite and non-negative"));
    }
    if (!std::isfinite(target.nzDefensive) || target.nzDefensive < 0.0) {
        return fail(QStringLiteral("Target defensive G must be finite and non-negative"));
    }
    if (!std::isfinite(atmosphere.rho) || atmosphere.rho <= 0.0 ||
        !std::isfinite(atmosphere.temperature) || atmosphere.temperature <= 0.0 ||
        !std::isfinite(atmosphere.pressure) || atmosphere.pressure <= 0.0 ||
        !std::isfinite(atmosphere.speedOfSound) || atmosphere.speedOfSound <= 0.0) {
        return fail(QStringLiteral("Atmosphere values must be finite and positive"));
    }
    if (!std::isfinite(weapon.referenceRangeNm) || weapon.referenceRangeNm <= 0.0 ||
        !std::isfinite(weapon.referenceAltitudeFeet) || weapon.referenceAltitudeFeet <= 0.0 ||
        !std::isfinite(weapon.referenceShooterMach) || weapon.referenceShooterMach < 0.0 ||
        !std::isfinite(weapon.baseMinimumRangeNm) || weapon.baseMinimumRangeNm <= 0.0 ||
        !std::isfinite(weapon.averageMissileSpeedKnots) || weapon.averageMissileSpeedKnots <= 0.0) {
        return fail(QStringLiteral("Weapon values must be finite and positive"));
    }
    return true;
}

} // namespace

std::optional<Geometry> calculateGeometry(const ShooterState &shooter, const TargetState &target,
                                          const Atmosphere &atmosphere, const WeaponModel &weapon,
                                          QString *error) {
    if (!validCommonInputs(shooter, target, atmosphere, weapon, error)) {
        return std::nullopt;
    }

    const Vec3 shooterToTarget = subtract(target.pos, shooter.pos);
    const Vec3 targetToShooter = subtract(shooter.pos, target.pos);
    const double rangeMeters = norm(shooterToTarget);
    if (!std::isfinite(rangeMeters) || rangeMeters <= 1e-9) {
        if (error != nullptr) {
            *error = QStringLiteral("Shooter and target must have non-zero separation");
        }
        return std::nullopt;
    }

    const Vec3 relativeVelocity = subtract(target.vel, shooter.vel);
    const double rangeRateMetersPerSecond = dot(shooterToTarget, relativeVelocity) / rangeMeters;
    const double targetSpeed = norm(target.vel);
    if (!std::isfinite(targetSpeed) || targetSpeed <= 1e-9) {
        if (error != nullptr) {
            *error = QStringLiteral("Target velocity is required for aspect");
        }
        return std::nullopt;
    }

    const double targetToShooterNorm = norm(targetToShooter);
    const double aspectCosine = std::clamp(
        dot(target.vel, targetToShooter) / (targetSpeed * targetToShooterNorm), -1.0, 1.0);
    const double aspect = std::acos(aspectCosine);

    const double horizontalRange = std::hypot(shooterToTarget.x, shooterToTarget.y);
    const double losAzimuth = std::atan2(shooterToTarget.y, shooterToTarget.x);
    const double losElevation = std::atan2(-shooterToTarget.z, horizontalRange);
    const Vec3 forward = forwardVector(shooter);
    const double boresightCosine =
        std::clamp(dot(forward, shooterToTarget) / rangeMeters, -1.0, 1.0);
    const double offBoresight = std::acos(boresightCosine);

    Geometry geometry;
    geometry.rangeNm = rangeMeters / units::MetersPerNauticalMile;
    geometry.rangeRateKnots = rangeRateMetersPerSecond / units::MetersPerSecondPerKnot;
    geometry.aspectRadians = aspect;
    geometry.antennaTrainAngleRadians = offBoresight;
    geometry.offBoresightRadians = offBoresight;
    geometry.losAzimuthRadians = losAzimuth;
    geometry.losElevationRadians = losElevation;
    geometry.altitudeDifferenceFeet = -(target.pos.z - shooter.pos.z) * units::FeetPerMeter;

    if (!std::isfinite(geometry.rangeNm) || geometry.rangeNm <= 0.0 ||
        !std::isfinite(geometry.rangeRateKnots) || !std::isfinite(geometry.aspectRadians) ||
        !std::isfinite(geometry.antennaTrainAngleRadians) ||
        !std::isfinite(geometry.offBoresightRadians) ||
        !std::isfinite(geometry.losAzimuthRadians) ||
        !std::isfinite(geometry.losElevationRadians) ||
        !std::isfinite(geometry.altitudeDifferenceFeet) || aspect < 0.0 || aspect > Pi) {
        if (error != nullptr) {
            *error = QStringLiteral("Derived DLZ geometry is invalid");
        }
        return std::nullopt;
    }
    return geometry;
}

} // namespace dlz
