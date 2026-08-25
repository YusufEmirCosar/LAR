
#include "viewer/statevalueformatter.h"

#include "domain/statefield.h"

#include <cmath>

namespace {
constexpr double RadiansToDegrees = 57.295779513082320876;
}

QString StateValueFormatter::format(int fieldId, double value) {
    QString unit;
    const StateField::Descriptor *descriptor = StateField::descriptor(fieldId);
    if (!descriptor)
        return QStringLiteral("%1 ").arg(value, 0, 'f', 3);
    switch (descriptor->unit) {
    case StateField::Unit::Radians:
        value *= RadiansToDegrees;
        unit = QStringLiteral("°");
        break;
    case StateField::Unit::KilometresPerHour:
        value /= 3.6;
        unit = QStringLiteral("m/s");
        break;
    case StateField::Unit::Metres:
        unit = QStringLiteral("m");
        break;
    case StateField::Unit::Seconds:
        unit = QStringLiteral("s");
        break;
    case StateField::Unit::NauticalMiles:
        unit = QStringLiteral("nm");
        break;
    case StateField::Unit::Degrees:
        unit = QStringLiteral("°");
        break;
    case StateField::Unit::Feet:
        unit = QStringLiteral("ft");
        break;
    }
    if (std::abs(value) < 1e-7)
        value = 0.0;
    return QStringLiteral("%1 %2").arg(value, 0, 'f', 3).arg(unit);
}
