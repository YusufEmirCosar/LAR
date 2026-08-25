#pragma once

/**
 * @file state.h
 * @brief Canonical in-memory aircraft, target, and LAR state structures.
 */

#include <QMetaType>

/**
 * @struct Plane
 * @brief Represents the position, attitude, and velocity of the aircraft.
 *
 * The protocol unit contract is fixed: location is latitude/longitude in
 * radians plus altitude in metres; Euler components are radians; velocity
 * components are kilometres per hour.
 */
struct Plane {
    double location[3]; ///< Latitude radians, longitude radians, altitude metres.
    double euler[3];    ///< Heading/yaw, pitch, and roll in radians.
    double velocity[3]; ///< Cartesian velocity components in kilometres per hour.
};

/**
 * @struct Target
 * @brief Target position and Launch Acceptable Region (LAR) parameters.
 */
struct Target {
    double iz_pos[3]; ///< In-zone latitude/longitude radians and radius metres.
    double ir_pos[3]; ///< In-range latitude/longitude radians and radius metres.
    double iz_theta1; ///< First in-zone boundary bearing in radians.
    double iz_theta2; ///< Second in-zone boundary bearing in radians.
    double iz_r1;     ///< In-zone inner radius in metres.
    double iz_r2;     ///< In-zone outer radius in metres.
    double ir_r;      ///< In-range radius in metres.
    double time;      ///< Producer scenario time in seconds.
};

static_assert(sizeof(Plane) == 9 * sizeof(double));
static_assert(sizeof(Target) == 12 * sizeof(double));

Q_DECLARE_METATYPE(Plane)
Q_DECLARE_METATYPE(Target)
