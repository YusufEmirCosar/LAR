
#include "domain/dlz/dlz_scenario_adapter.h"

#include "domain/dlz/dlz_units.h"

#include <cmath>

namespace dlz {
namespace {

constexpr double Pi = 3.1415926535897932384626433832795;

QString solveErrorMessage(DlzSolveError error) {
    switch (error) {
    case DlzSolveError::InvalidInput:
        return QStringLiteral("DLZ solver input is invalid");
    case DlzSolveError::UnsupportedOrderedDomain:
        return QStringLiteral("DLZ toy model input is outside its ordered domain");
    case DlzSolveError::NumericOverflow:
        return QStringLiteral("DLZ toy model calculation exceeded its finite numeric domain");
    case DlzSolveError::InvalidDerivedOrdering:
        return QStringLiteral("DLZ toy model result is not strictly ordered");
    case DlzSolveError::None:
        break;
    }
    return {};
}

bool validInputs(const ScenarioInputs &inputs, QString *error) {
    const auto fail = [error](const QString &message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    if (!std::isfinite(inputs.rangeNm) || inputs.rangeNm <= 0.0 ||
        !std::isfinite(inputs.aspectDegrees) || inputs.aspectDegrees < 0.0 ||
        inputs.aspectDegrees > 180.0 || !std::isfinite(inputs.altitudeFeet) ||
        inputs.altitudeFeet < 0.0 || !std::isfinite(inputs.shooterMach) ||
        inputs.shooterMach < 0.0 || !std::isfinite(inputs.targetMach) || inputs.targetMach < 0.0) {
        return fail(QStringLiteral("Range, aspect, altitude, and Mach controls are invalid"));
    }
    return true;
}

} // namespace

bool ScenarioAdapter::build(const ScenarioInputs &inputs, ScenarioFrame *frame, QString *error) {
    if (frame == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("DLZ scenario output is null");
        }
        return false;
    }
    if (!validInputs(inputs, error)) {
        return false;
    }

    ScenarioFrame result;
    result.weapon = WeaponModel{};
    result.atmosphere = Atmosphere{};
    result.shooter.mach = inputs.shooterMach;
    result.shooter.psi = 0.0;
    result.shooter.theta = 0.0;
    result.shooter.phi = 0.0;
    result.shooter.pos = {0.0, 0.0, -inputs.altitudeFeet / units::FeetPerMeter};

    const double aspectRadians = inputs.aspectDegrees * units::RadiansPerDegree;
    const double targetSpeed = inputs.targetMach * result.atmosphere.speedOfSound;
    result.shooter.vel = {inputs.shooterMach * result.atmosphere.speedOfSound, 0.0, 0.0};
    result.target.pos = {inputs.rangeNm * units::MetersPerNauticalMile, 0.0, result.shooter.pos.z};
    // Target velocity is selected so target-velocity versus target-to-shooter
    // LOS produces the requested aspect exactly.
    result.target.vel = {targetSpeed * std::cos(Pi - aspectRadians),
                         targetSpeed * std::sin(Pi - aspectRadians), 0.0};
    result.target.mach = inputs.targetMach;
    result.target.psi = Pi - aspectRadians;
    result.target.nzDefensive = 9.0;

    QString geometryError;
    const auto geometry = calculateGeometry(result.shooter, result.target, result.atmosphere,
                                            result.weapon, &geometryError);
    if (!geometry.has_value()) {
        if (error != nullptr) {
            *error = geometryError;
        }
        return false;
    }
    if (std::abs(geometry->aspectRadians - aspectRadians) > 1e-9) {
        if (error != nullptr) {
            *error = QStringLiteral("Scenario adapter could not realize requested aspect");
        }
        return false;
    }

    result.geometry = *geometry;
    // Keep the exact formula values available to the UI even when the strict
    // solver-domain guard rejects this input.  The workspace uses these only
    // as readouts while the invalid frame is withheld from the renderer.
    result.solution = evaluateToyModel(result.shooter, result.geometry, result.weapon);

    const DlzSolveResult solution =
        solve(result.shooter, result.target, result.atmosphere, *geometry, result.weapon);
    if (!solution) {
        *frame = result;
        if (error != nullptr) {
            *error = solveErrorMessage(solution.error);
        }
        return false;
    }
    result.solution = *solution.solution;
    *frame = result;
    return true;
}

} // namespace dlz
