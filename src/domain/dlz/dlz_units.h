#pragma once

/**
 * @file dlz_units.h
 * @brief Compile-time conversion constants used by the DLZ domain.
 */

/** @brief Small unit constants shared by the DLZ domain helpers. */
namespace dlz::units {

constexpr double MetersPerNauticalMile = 1852.0;
constexpr double FeetPerMeter = 3.280839895013123;
constexpr double MetersPerSecondPerKnot = 0.514444444444444;
constexpr double DegreesPerRadian = 57.2957795130823208768;
constexpr double RadiansPerDegree = 0.01745329251994329577;

} // namespace dlz::units
