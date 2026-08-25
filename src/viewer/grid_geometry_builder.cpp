
#include "viewer/grid_geometry_builder.h"

#include <cmath>

double GridGeometryBuilder::gridStep(double targetSpacing) noexcept {
    return std::exp2(std::round(std::log2(targetSpacing)));
}

double GridGeometryBuilder::scaleBarDistance(double maximumDistance) noexcept {
    const double magnitude = std::pow(10.0, std::floor(std::log10(maximumDistance)));
    const double normalized = maximumDistance / magnitude;
    return (normalized >= 5.0 ? 5.0 : normalized >= 2.0 ? 2.0 : 1.0) * magnitude;
}

QString GridGeometryBuilder::formatDistance(double meters) {
    if (meters >= 1000.0) {
        const double kilometers = meters / 1000.0;
        return QStringLiteral("%1 km").arg(kilometers, 0, 'f', kilometers < 10.0 ? 1 : 0);
    }
    return QStringLiteral("%1 m").arg(meters, 0, 'f', meters < 10.0 ? 1 : 0);
}
