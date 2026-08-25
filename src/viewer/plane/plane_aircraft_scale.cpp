
#include "viewer/plane/plane_aircraft_scale.h"

#include <cmath>

double PlaneAircraftScale::metersPerSceneUnit(float forwardExtentSceneUnits) noexcept {
    const double normalizedLength = static_cast<double>(forwardExtentSceneUnits);
    if (!std::isfinite(normalizedLength) || normalizedLength <= 1.0e-9) {
        return DefaultMetersPerSceneUnit;
    }
    return F16LengthMeters / normalizedLength;
}
