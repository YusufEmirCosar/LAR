#pragma once

/**
 * @file dlz_geometry.h
 * @brief Pure derivation of engagement geometry from shooter and target state.
 */

#include "domain/dlz/dlz_types.h"

#include <QString>

#include <optional>

namespace dlz {

/**
 * Derives finite range, rates, angles, and altitude separation.
 *
 * @param shooter Shooter position, velocity, and attitude.
 * @param target Target position and velocity.
 * @param atmosphere Atmospheric conversion values.
 * @param weapon Weapon model retained for API symmetry and validation.
 * @param error Optional human-readable rejection reason.
 * @return Complete geometry, or no value when an input or derived value is invalid.
 */
std::optional<Geometry> calculateGeometry(const ShooterState &shooter, const TargetState &target,
                                          const Atmosphere &atmosphere, const WeaponModel &weapon,
                                          QString *error = nullptr);

} // namespace dlz
