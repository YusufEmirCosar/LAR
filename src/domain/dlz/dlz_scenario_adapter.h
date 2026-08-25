#pragma once

/**
 * @file dlz_scenario_adapter.h
 * @brief Deterministic adapter from compact scenario controls to DLZ domain state.
 */

#include "domain/dlz/dlz_geometry.h"
#include "domain/dlz/dlz_solver.h"

#include <QString>

namespace dlz {

/** Minimal editable values accepted from the local DLZ control panel. */
struct ScenarioInputs {
    double rangeNm = 20.0;
    double aspectDegrees = 0.0;
    double altitudeFeet = 30000.0;
    double shooterMach = 0.90;
    double targetMach = 0.95;
};

/** Complete, internally consistent domain frame produced from scenario inputs. */
struct ScenarioFrame {
    ShooterState shooter;
    TargetState target;
    Atmosphere atmosphere;
    WeaponModel weapon;
    Geometry geometry;
    Solution solution;
};

/** @brief Builds deterministic state fixtures for the toy DLZ solver. */
class ScenarioAdapter final {
  public:
    static bool build(const ScenarioInputs &inputs, ScenarioFrame *frame, QString *error = nullptr);
};

} // namespace dlz
