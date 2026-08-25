#pragma once

/**
 * @file lar_zone_input_validator.h
 * @brief Projection-neutral validation and normalization of LAR zone inputs.
 */

#include "domain/state.h"
#include "viewer/lar_geodesic_geometry.h"

#include <QBitArray>

#include <optional>

enum class LarZoneKind {
    InRange,
    InZone,
};

/** Projection-neutral, validated input for one radial geodesic patch. */
struct LarZoneDefinition final {
    LarZoneKind kind = LarZoneKind::InRange;
    GeoCoordinateRadians center{};
    double innerRadiusMeters = 0.0;
    double outerRadiusMeters = 0.0;
    double startBearingRadians = 0.0;
    double spanRadians = 0.0;
};

struct LarZoneValidationResult final {
    std::optional<LarZoneDefinition> inRange;
    std::optional<LarZoneDefinition> inZone;
    bool inputRejected = false;
};

/** Validates field presence, finite values, domain bounds, and radius limits. */
class LarZoneInputValidator final {
  public:
    [[nodiscard]] LarZoneValidationResult validate(const Target &target,
                                                   const QBitArray &availableFields) const noexcept;
};
