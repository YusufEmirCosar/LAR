#pragma once

/**
 * @file statefield.h
 * @brief Stable identifiers and access helpers for every scalar state field.
 */

#include "domain/state.h"

#include <QString>

#include <array>
#include <optional>

struct DecodedState;
namespace dlz {
struct TelemetryInputs;
}

/** @brief Reflective access to scalar members of Plane and Target. */
class StateField {
  public:
    /** Stable bit positions shared by mappings, availability masks, and views. */
    enum Id {
        Location0,
        Location1,
        Location2,
        Euler0,
        Euler1,
        Euler2,
        Velocity0,
        Velocity1,
        Velocity2,
        IzPos0,
        IzPos1,
        IzPos2,
        IrPos0,
        IrPos1,
        IrPos2,
        IzTheta1,
        IzTheta2,
        IzR1,
        IzR2,
        IrR,
        Time,
        DlzRangeNm,
        DlzAspectDegrees,
        DlzAltitudeFeet,
        Count
    };

    /** Physical unit carried by a descriptor and used by presentation formatting. */
    enum class Unit {
        Radians,
        KilometresPerHour,
        Metres,
        Seconds,
        NauticalMiles,
        Degrees,
        Feet,
    };

    /** Scalar validation owned by one field; cross-field rules remain in StateValidator. */
    enum class ValidationCategory {
        Finite,
        LatitudeRadians,
        LongitudeRadians,
        NonNegative,
        Positive,
        DlzRangeNauticalMiles,
        DlzAspectDegrees,
        DlzAltitudeFeet,
    };

    /** Immutable metadata and access functions for one scalar field. */
    struct Descriptor final {
        using Getter = double (*)(const Plane &, const Target &,
                                  const dlz::TelemetryInputs &) noexcept;
        using Setter = void (*)(Plane &, Target &, dlz::TelemetryInputs &, double) noexcept;

        const char *mappingName;
        int mappingIndex;
        const char *memberName;
        const char *presentationName;
        Unit unit;
        ValidationCategory validation;
        Getter getter;
        Setter setter;
    };

    using Registry = std::array<Descriptor, Count>;

    static const Descriptor *descriptor(int id) noexcept;
    static const Registry &all() noexcept;

    static int resolve(const QString &name, int index);
    static QString displayName(int id);
    static QString presentationName(int id);
    static double value(const Plane &plane, const Target &target, int id);
    static double value(const DecodedState &state, int id);
    static std::optional<double> tryValue(const Plane &plane, const Target &target,
                                          int id) noexcept;
    static std::optional<double> tryValue(const DecodedState &state, int id) noexcept;
    static bool setValue(Plane *plane, Target *target, int id, double value);
    static bool setValue(DecodedState *state, int id, double value);
};
