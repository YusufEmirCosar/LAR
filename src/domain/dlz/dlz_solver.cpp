
#include "domain/dlz/dlz_solver.h"

#include "domain/dlz/dlz_units.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace dlz {
namespace {
double aspectFactor(double aspectRadians) {
    return 0.30 + 0.70 * (1.0 + std::cos(aspectRadians)) / 2.0;
}
double altitudeFactor(double altitudeFeet) {
    return std::clamp(1.0 + 0.020 * (altitudeFeet / 1000.0 - 30.0), 0.6, 1.6);
}
double machFactor(double shooterMach) {
    return 0.70 + 0.35 * shooterMach;
}

bool finiteGeometry(const Geometry &geometry) {
    return std::isfinite(geometry.rangeNm) && std::isfinite(geometry.rangeRateKnots) &&
           std::isfinite(geometry.aspectRadians) &&
           std::isfinite(geometry.antennaTrainAngleRadians) &&
           std::isfinite(geometry.offBoresightRadians) &&
           std::isfinite(geometry.losAzimuthRadians) &&
           std::isfinite(geometry.losElevationRadians) &&
           std::isfinite(geometry.altitudeDifferenceFeet);
}
bool finiteVector(const Vec3 &vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

bool validSolutionOrder(const Solution &solution) {
    return std::isfinite(solution.minimumRangeNm) && std::isfinite(solution.turnAndRunRangeNm) &&
           std::isfinite(solution.noEscapeRangeNm) && std::isfinite(solution.interceptRangeNm) &&
           std::isfinite(solution.aerodynamicMaximumRangeNm) &&
           std::isfinite(solution.timeOfFlightSeconds) &&
           solution.minimumRangeNm < solution.turnAndRunRangeNm &&
           solution.turnAndRunRangeNm < solution.noEscapeRangeNm &&
           solution.noEscapeRangeNm < solution.interceptRangeNm &&
           solution.interceptRangeNm < solution.aerodynamicMaximumRangeNm;
}

DlzSolveError orderingError(double aspectRadians, double altitudeFeet, double shooterMach,
                            const WeaponModel &weapon) noexcept {
    const double aero = weapon.referenceRangeNm * aspectFactor(aspectRadians) *
                        altitudeFactor(altitudeFeet) * machFactor(shooterMach);
    const double noEscape = aero * (0.44 * aspectFactor(aspectRadians) + 0.08);
    const double turnAndRun = 0.60 * noEscape;
    if (!std::isfinite(aero) || !std::isfinite(noEscape) || !std::isfinite(turnAndRun)) {
        return DlzSolveError::NumericOverflow;
    }
    if (turnAndRun <= weapon.baseMinimumRangeNm) {
        return DlzSolveError::UnsupportedOrderedDomain;
    }
    return DlzSolveError::None;
}

} // namespace

bool supportsOrdering(double aspectRadians, double altitudeFeet, double shooterMach,
                      const WeaponModel &weapon) {
    if (!std::isfinite(aspectRadians) || aspectRadians < 0.0 ||
        aspectRadians > 3.14159265358979323846 || !std::isfinite(altitudeFeet) ||
        altitudeFeet < 0.0 || !std::isfinite(shooterMach) || shooterMach < 0.0 ||
        !std::isfinite(weapon.referenceRangeNm) || !std::isfinite(weapon.referenceAltitudeFeet) ||
        !std::isfinite(weapon.referenceShooterMach) || !std::isfinite(weapon.baseMinimumRangeNm) ||
        !std::isfinite(weapon.averageMissileSpeedKnots) || weapon.referenceRangeNm <= 0.0 ||
        weapon.referenceAltitudeFeet <= 0.0 || weapon.referenceShooterMach < 0.0 ||
        weapon.baseMinimumRangeNm <= 0.0 || weapon.averageMissileSpeedKnots <= 0.0) {
        return false;
    }
    return orderingError(aspectRadians, altitudeFeet, shooterMach, weapon) == DlzSolveError::None;
}

Solution evaluateToyModel(const ShooterState &shooter, const Geometry &geometry,
                          const WeaponModel &weapon) noexcept {
    const double shooterAltitudeFeet = -shooter.pos.z * units::FeetPerMeter;
    const double fAspect = aspectFactor(geometry.aspectRadians);
    const double fAlt = altitudeFactor(shooterAltitudeFeet);
    const double fMach = machFactor(shooter.mach);

    Solution solution;
    solution.aerodynamicMaximumRangeNm = weapon.referenceRangeNm * fAspect * fAlt * fMach;
    solution.interceptRangeNm = 0.83 * solution.aerodynamicMaximumRangeNm;
    solution.noEscapeRangeNm = solution.aerodynamicMaximumRangeNm * (0.44 * fAspect + 0.08);
    solution.turnAndRunRangeNm = 0.60 * solution.noEscapeRangeNm;
    solution.minimumRangeNm = weapon.baseMinimumRangeNm;
    solution.timeOfFlightSeconds = (geometry.rangeNm / weapon.averageMissileSpeedKnots) * 3600.0;
    solution.inEnvelope =
        geometry.rangeNm > solution.minimumRangeNm && geometry.rangeNm <= solution.interceptRangeNm;
    solution.shootCue = solution.inEnvelope;
    return solution;
}

DlzSolveResult solve(const ShooterState &shooter, const TargetState &target,
                     const Atmosphere &atmosphere, const Geometry &geometry,
                     const WeaponModel &weapon) noexcept {
    if (!finiteVector(shooter.pos) || !finiteVector(shooter.vel) || !finiteVector(target.pos) ||
        !finiteVector(target.vel) || !std::isfinite(shooter.mach) || shooter.mach < 0.0 ||
        !std::isfinite(target.mach) || target.mach < 0.0 || !std::isfinite(target.nzDefensive) ||
        target.nzDefensive < 0.0 || !std::isfinite(shooter.psi) || !std::isfinite(shooter.theta) ||
        !std::isfinite(shooter.phi) || !std::isfinite(target.psi) ||
        !std::isfinite(atmosphere.rho) || atmosphere.rho <= 0.0 ||
        !std::isfinite(atmosphere.temperature) || atmosphere.temperature <= 0.0 ||
        !std::isfinite(atmosphere.pressure) || atmosphere.pressure <= 0.0 ||
        !std::isfinite(atmosphere.speedOfSound) || atmosphere.speedOfSound <= 0.0 ||
        !finiteGeometry(geometry) || geometry.rangeNm <= 0.0 ||
        !std::isfinite(weapon.referenceRangeNm) || weapon.referenceRangeNm <= 0.0 ||
        !std::isfinite(weapon.referenceAltitudeFeet) || weapon.referenceAltitudeFeet <= 0.0 ||
        !std::isfinite(weapon.referenceShooterMach) || weapon.referenceShooterMach < 0.0 ||
        !std::isfinite(weapon.baseMinimumRangeNm) || weapon.baseMinimumRangeNm <= 0.0 ||
        !std::isfinite(weapon.averageMissileSpeedKnots) || weapon.averageMissileSpeedKnots <= 0.0) {
        return {{}, DlzSolveError::InvalidInput};
    }
    // The altitude input is carried by the scenario adapter in the shooter's
    // NED z coordinate.  The solver uses the declared mean-sea-level datum.
    const double shooterAltitudeFeet = -shooter.pos.z * units::FeetPerMeter;
    const DlzSolveError orderError =
        orderingError(geometry.aspectRadians, shooterAltitudeFeet, shooter.mach, weapon);
    if (orderError != DlzSolveError::None) {
        return {{}, orderError};
    }

    const Solution solution = evaluateToyModel(shooter, geometry, weapon);

    if (!std::isfinite(solution.aerodynamicMaximumRangeNm) ||
        !std::isfinite(solution.interceptRangeNm) || !std::isfinite(solution.noEscapeRangeNm) ||
        !std::isfinite(solution.turnAndRunRangeNm) || !std::isfinite(solution.minimumRangeNm) ||
        !std::isfinite(solution.timeOfFlightSeconds)) {
        return {{}, DlzSolveError::NumericOverflow};
    }
    if (!validSolutionOrder(solution)) {
        return {{}, DlzSolveError::InvalidDerivedOrdering};
    }
    return {solution, DlzSolveError::None};
}

} // namespace dlz
