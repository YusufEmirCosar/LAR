
#include "domain/statevalidator.h"
#include "domain/statefield.h"

#include <cmath>

namespace {
constexpr double Pi = 3.14159265358979323846;

bool validateField(const StateField::Descriptor &field, double value, QString *error) {
    const QString member = QString::fromLatin1(field.memberName);
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    if (!std::isfinite(value)) {
        return fail(QStringLiteral("%1 is not finite").arg(member));
    }

    using Category = StateField::ValidationCategory;
    switch (field.validation) {
    case Category::Finite:
        return true;
    case Category::LatitudeRadians:
        return value >= -Pi * 0.5 && value <= Pi * 0.5
                   ? true
                   : fail(QStringLiteral("%1 is outside [-pi/2, pi/2]").arg(member));
    case Category::LongitudeRadians:
        return value >= -Pi && value <= Pi
                   ? true
                   : fail(QStringLiteral("%1 is outside [-pi, pi]").arg(member));
    case Category::NonNegative:
        return value >= 0.0 ? true : fail(QStringLiteral("%1 must be non-negative").arg(member));
    case Category::Positive:
        return value > 0.0 ? true : fail(QStringLiteral("%1 must be positive").arg(member));
    case Category::DlzRangeNauticalMiles:
        return value >= 0.1 && value <= 60.0
                   ? true
                   : fail(QStringLiteral("DLZ.range_nm must be within [0.1, 60.0] NM"));
    case Category::DlzAspectDegrees:
        return value >= 0.0 && value <= 180.0
                   ? true
                   : fail(QStringLiteral("DLZ.aspect_deg must be within [0, 180] degrees"));
    case Category::DlzAltitudeFeet:
        return value >= 0.0 && value <= 60000.0
                   ? true
                   : fail(QStringLiteral("DLZ.altitude_ft must be within [0, 60000] feet"));
    }
    return fail(QStringLiteral("Unknown validation category for %1").arg(member));
}
} // namespace

bool StateValidator::validate(const Plane &plane, const Target &target,
                              const QBitArray &availableFields, QString *error) {
    const auto isAvailable = [&availableFields](int id) {
        return id >= 0 && id < availableFields.size() && availableFields.testBit(id);
    };
    for (int id = StateField::Location0; id <= StateField::Time; ++id) {
        if (!isAvailable(id))
            continue;
        const StateField::Descriptor *field = StateField::descriptor(id);
        const auto value = StateField::tryValue(plane, target, id);
        if (!field || !value || !validateField(*field, *value, error)) {
            return false;
        }
    }
    if (isAvailable(StateField::IzR1) && isAvailable(StateField::IzR2) &&
        target.iz_r2 <= target.iz_r1) {
        if (error)
            *error = QStringLiteral("Target ranges must satisfy iz_r1 < iz_r2");
        return false;
    }
    return true;
}

bool StateValidator::validate(const DecodedState &state, QString *error) {
    const auto &fields = state.availableFields;
    if (!validate(state.plane, state.target, fields, error)) {
        return false;
    }
    const auto isAvailable = [&fields](int id) {
        return id >= 0 && id < fields.size() && fields.testBit(id);
    };
    for (int id = StateField::DlzRangeNm; id <= StateField::DlzAltitudeFeet; ++id) {
        if (!isAvailable(id))
            continue;
        const StateField::Descriptor *field = StateField::descriptor(id);
        const auto value = StateField::tryValue(state, id);
        if (!field || !value || !validateField(*field, *value, error)) {
            return false;
        }
    }
    return true;
}
