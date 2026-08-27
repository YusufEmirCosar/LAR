#pragma once

/**
 * @file dlz_types.h
 * @brief Dedicated Dynamic Launch Zone domain values.
 *
 * These types deliberately do not reuse the packet monitor's ground-LAR
 * Plane/Target structures.  They are the small, self-contained teaching model
 * used by the DLZ HUD workspace.
 */

namespace dlz {

/** Cartesian vector whose axes and units are defined by the owning value. */
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/**
 * Shooter kinematics in a local North-East-Down (NED) frame.
 *
 * Position components are metres and velocity components are metres per
 * second: x is north, y is east, and z is down. `psi` is yaw clockwise from
 * north, `theta` is pitch positive nose-up, and `phi` is roll; all attitudes
 * are radians. Mach is dimensionless.
 */
struct ShooterState {
    Vec3 pos;
    Vec3 vel;
    double mach = 0.0;
    double psi = 0.0;
    double theta = 0.0;
    double phi = 0.0;
};

/**
 * Target kinematics in the shooter's local North-East-Down (NED) frame.
 *
 * Position and velocity use metres and metres per second. `psi` is heading in
 * radians clockwise from north, and `nzDefensive` is the non-negative normal
 * load factor in g used by the teaching model.
 */
struct TargetState {
    Vec3 pos;
    Vec3 vel;
    double mach = 0.0;
    double psi = 0.0;
    double nzDefensive = 0.0;
};

/** @brief The three packet/replay inputs consumed by the DLZ workspace. */
struct TelemetryInputs {
    double rangeNm = 0.0;       ///< Slant range in nautical miles.
    double aspectDegrees = 0.0; ///< Target aspect in the closed range 0..180 degrees.
    double altitudeFeet = 0.0;  ///< Shooter altitude in feet.
};

/** Atmospheric values used to convert Mach to velocity. */
struct Atmosphere {
    double rho = 1.225;            ///< Density in kilograms per cubic metre.
    double temperature = 288.15;   ///< Absolute temperature in kelvin.
    double pressure = 101325.0;    ///< Static pressure in pascals.
    double speedOfSound = 340.294; ///< Local speed of sound in metres per second.
};

/** Derived relative engagement geometry in explicit aviation units. */
struct Geometry {
    double rangeNm = 0.0;                  ///< Shooter-to-target slant range.
    double rangeRateKnots = 0.0;           ///< Positive when opening; negative when closing.
    double aspectRadians = 0.0;            ///< 0 is head-on; pi is target heading away.
    double antennaTrainAngleRadians = 0.0; ///< Angle between boresight and target LOS.
    double offBoresightRadians = 0.0;      ///< Alias of antenna train angle in this model.
    double losAzimuthRadians = 0.0;        ///< From north toward east in the local NED frame.
    double losElevationRadians = 0.0;      ///< Positive above the local horizontal plane.
    double altitudeDifferenceFeet = 0.0;   ///< Target altitude minus shooter altitude.
};

/** Tunable constants for the intentionally simplified teaching model. */
struct WeaponModel {
    double referenceRangeNm = 32.0;
    double referenceAltitudeFeet = 30000.0;
    double referenceShooterMach = 0.90;
    double baseMinimumRangeNm = 1.8;
    double averageMissileSpeedKnots = 1600.0;
};

/** Ordered launch-zone ranges and presentation cues produced by solve(). */
struct Solution {
    double aerodynamicMaximumRangeNm = 0.0;
    double interceptRangeNm = 0.0;
    double noEscapeRangeNm = 0.0;
    double turnAndRunRangeNm = 0.0;
    double minimumRangeNm = 0.0;
    double timeOfFlightSeconds = 0.0;
    bool inEnvelope = false;
    bool shootCue = false;
};

/** Temporal phase used by the HUD presentation state machine. */
enum class HudMode { NoTrack, Prelaunch, Inflight, Terminal };

/** Filtered and scaled values ready for stateless HUD rendering. */
struct HudState {
    double currentRangeNm = 0.0;
    double rangeRateKnots = 0.0;
    double scaleMaximumNm = 40.0;
    double scaleMinimumNm = 0.0;
    bool caretParked = false;
    HudMode mode = HudMode::NoTrack;
    double timeToImpactRemainingSeconds = 0.0;
    bool shootFlash = false;
};

} // namespace dlz
