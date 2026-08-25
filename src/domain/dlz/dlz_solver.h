#pragma once

/**
 * @file dlz_solver.h
 * @brief Guarded toy-model Dynamic Launch Zone solver API.
 */

#include "domain/dlz/dlz_types.h"

#include <optional>

namespace dlz {

/** Stable reason why solve() did not return a presentation-safe solution. */
enum class DlzSolveError {
    None = 0,
    InvalidInput,
    UnsupportedOrderedDomain,
    NumericOverflow,
    InvalidDerivedOrdering,
};

/** Value-or-error return type that prevents invalid ranges reaching rendering. */
struct DlzSolveResult final {
    std::optional<Solution> solution;
    DlzSolveError error = DlzSolveError::None;

    [[nodiscard]] bool hasValue() const noexcept {
        return solution.has_value();
    }
    /**
     * @brief Reports whether this result contains a valid solution.
     *
     * @details The conversion mirrors hasValue() so the result can be used directly
     * in conditional expressions.
     *
     * @return True when a solution is present; false when the result contains an error.
     */
    explicit operator bool() const noexcept {
        return hasValue();
    }
};

/** Returns whether the toy equations are monotonic in their supported domain. */
bool supportsOrdering(double aspectRadians, double altitudeFeet, double shooterMach,
                      const WeaponModel &weapon);

/**
 * @brief Evaluates the exact toy equations without applying the ordering guard.
 *
 * @details This is used only for diagnostic readouts when an input is outside
 * the model's supported domain.  It must never be treated as a valid solver
 * result or passed to the renderer; use solve() for that contract.
 * @param[in] shooter Shooter state supplying altitude and Mach.
 * @param[in] geometry Derived geometry supplying range and aspect.
 * @param[in] weapon Weapon-model parameters used by the toy equations.
 * @return The unvalidated toy-model values corresponding to the supplied state.
 */
Solution evaluateToyModel(const ShooterState &shooter, const Geometry &geometry,
                          const WeaponModel &weapon) noexcept;

/**
 * Produces an ordered, finite launch-zone solution for supported inputs.
 * @return A solution or a categorized error; invalid partial values are never returned.
 */
DlzSolveResult solve(const ShooterState &shooter, const TargetState &target,
                     const Atmosphere &atmosphere, const Geometry &geometry,
                     const WeaponModel &weapon) noexcept;

} // namespace dlz
