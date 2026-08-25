
#include "domain/statefield.h"

#include "domain/decoded_state.h"

#include <limits>

namespace {
using Category = StateField::ValidationCategory;
using Descriptor = StateField::Descriptor;
using Unit = StateField::Unit;

template <int Id>
double readField(const Plane &plane, const Target &target,
                 const dlz::TelemetryInputs &dlzInputs) noexcept {
    if constexpr (Id >= StateField::Location0 && Id <= StateField::Location2) {
        return plane.location[Id - StateField::Location0];
    } else if constexpr (Id >= StateField::Euler0 && Id <= StateField::Euler2) {
        return plane.euler[Id - StateField::Euler0];
    } else if constexpr (Id >= StateField::Velocity0 && Id <= StateField::Velocity2) {
        return plane.velocity[Id - StateField::Velocity0];
    } else if constexpr (Id >= StateField::IzPos0 && Id <= StateField::IzPos2) {
        return target.iz_pos[Id - StateField::IzPos0];
    } else if constexpr (Id >= StateField::IrPos0 && Id <= StateField::IrPos2) {
        return target.ir_pos[Id - StateField::IrPos0];
    } else if constexpr (Id == StateField::IzTheta1) {
        return target.iz_theta1;
    } else if constexpr (Id == StateField::IzTheta2) {
        return target.iz_theta2;
    } else if constexpr (Id == StateField::IzR1) {
        return target.iz_r1;
    } else if constexpr (Id == StateField::IzR2) {
        return target.iz_r2;
    } else if constexpr (Id == StateField::IrR) {
        return target.ir_r;
    } else if constexpr (Id == StateField::Time) {
        return target.time;
    } else if constexpr (Id == StateField::DlzRangeNm) {
        return dlzInputs.rangeNm;
    } else if constexpr (Id == StateField::DlzAspectDegrees) {
        return dlzInputs.aspectDegrees;
    } else {
        static_assert(Id == StateField::DlzAltitudeFeet, "Unhandled StateField getter");
        return dlzInputs.altitudeFeet;
    }
}

template <int Id>
void writeField(Plane &plane, Target &target, dlz::TelemetryInputs &dlzInputs,
                double value) noexcept {
    if constexpr (Id >= StateField::Location0 && Id <= StateField::Location2) {
        plane.location[Id - StateField::Location0] = value;
    } else if constexpr (Id >= StateField::Euler0 && Id <= StateField::Euler2) {
        plane.euler[Id - StateField::Euler0] = value;
    } else if constexpr (Id >= StateField::Velocity0 && Id <= StateField::Velocity2) {
        plane.velocity[Id - StateField::Velocity0] = value;
    } else if constexpr (Id >= StateField::IzPos0 && Id <= StateField::IzPos2) {
        target.iz_pos[Id - StateField::IzPos0] = value;
    } else if constexpr (Id >= StateField::IrPos0 && Id <= StateField::IrPos2) {
        target.ir_pos[Id - StateField::IrPos0] = value;
    } else if constexpr (Id == StateField::IzTheta1) {
        target.iz_theta1 = value;
    } else if constexpr (Id == StateField::IzTheta2) {
        target.iz_theta2 = value;
    } else if constexpr (Id == StateField::IzR1) {
        target.iz_r1 = value;
    } else if constexpr (Id == StateField::IzR2) {
        target.iz_r2 = value;
    } else if constexpr (Id == StateField::IrR) {
        target.ir_r = value;
    } else if constexpr (Id == StateField::Time) {
        target.time = value;
    } else if constexpr (Id == StateField::DlzRangeNm) {
        dlzInputs.rangeNm = value;
    } else if constexpr (Id == StateField::DlzAspectDegrees) {
        dlzInputs.aspectDegrees = value;
    } else {
        static_assert(Id == StateField::DlzAltitudeFeet, "Unhandled StateField setter");
        dlzInputs.altitudeFeet = value;
    }
}

#define LAR_FIELD(ID, MAP, INDEX, MEMBER, LABEL, UNIT, VALIDATION)                                 \
    Descriptor {                                                                                   \
        MAP, INDEX, MEMBER, LABEL, Unit::UNIT, Category::VALIDATION, &readField<StateField::ID>,   \
            &writeField<StateField::ID>                                                            \
    }

constexpr StateField::Registry Descriptors{{
    LAR_FIELD(Location0, "location", 0, "Plane.location[0]", "Latitude", Radians, LatitudeRadians),
    LAR_FIELD(Location1, "location", 1, "Plane.location[1]", "Longitude", Radians,
              LongitudeRadians),
    LAR_FIELD(Location2, "location", 2, "Plane.location[2]", "Altitude", Metres, Finite),
    LAR_FIELD(Euler0, "euler", 0, "Plane.euler[0]", "Heading (Yaw)", Radians, Finite),
    LAR_FIELD(Euler1, "euler", 1, "Plane.euler[1]", "Pitch", Radians, Finite),
    LAR_FIELD(Euler2, "euler", 2, "Plane.euler[2]", "Roll", Radians, Finite),
    LAR_FIELD(Velocity0, "velocity", 0, "Plane.velocity[0]", "X Axis", KilometresPerHour, Finite),
    LAR_FIELD(Velocity1, "velocity", 1, "Plane.velocity[1]", "Y Axis", KilometresPerHour, Finite),
    LAR_FIELD(Velocity2, "velocity", 2, "Plane.velocity[2]", "Z Axis", KilometresPerHour, Finite),
    LAR_FIELD(IzPos0, "iz_pos", 0, "Target.iz_pos[0]", "Latitude", Radians, LatitudeRadians),
    LAR_FIELD(IzPos1, "iz_pos", 1, "Target.iz_pos[1]", "Longitude", Radians, LongitudeRadians),
    LAR_FIELD(IzPos2, "iz_pos", 2, "Target.iz_pos[2]", "Radius", Metres, Finite),
    LAR_FIELD(IrPos0, "ir_pos", 0, "Target.ir_pos[0]", "Latitude", Radians, LatitudeRadians),
    LAR_FIELD(IrPos1, "ir_pos", 1, "Target.ir_pos[1]", "Longitude", Radians, LongitudeRadians),
    LAR_FIELD(IrPos2, "ir_pos", 2, "Target.ir_pos[2]", "Radius", Metres, Finite),
    LAR_FIELD(IzTheta1, "iz_theta1", 0, "Target.iz_theta1", "Start Angle", Radians, Finite),
    LAR_FIELD(IzTheta2, "iz_theta2", 0, "Target.iz_theta2", "End Angle", Radians, Finite),
    LAR_FIELD(IzR1, "iz_r1", 0, "Target.iz_r1", "Minimum Range", Metres, NonNegative),
    LAR_FIELD(IzR2, "iz_r2", 0, "Target.iz_r2", "Maximum Range", Metres, Positive),
    LAR_FIELD(IrR, "ir_r", 0, "Target.ir_r", "Maximum Range", Metres, Positive),
    LAR_FIELD(Time, "time", 0, "Target.time", "Timestamp", Seconds, Finite),
    LAR_FIELD(DlzRangeNm, "dlz_range_nm", 0, "DLZ.range_nm", "Range", NauticalMiles,
              DlzRangeNauticalMiles),
    LAR_FIELD(DlzAspectDegrees, "dlz_aspect_deg", 0, "DLZ.aspect_deg", "Aspect", Degrees,
              DlzAspectDegrees),
    LAR_FIELD(DlzAltitudeFeet, "dlz_altitude_ft", 0, "DLZ.altitude_ft", "Altitude", Feet,
              DlzAltitudeFeet),
}};

#undef LAR_FIELD

constexpr bool hasCompleteDescriptors() {
    for (const Descriptor &field : Descriptors) {
        if (!field.mappingName || field.mappingName[0] == '\0' || field.mappingIndex < 0 ||
            !field.memberName || field.memberName[0] == '\0' || !field.presentationName ||
            field.presentationName[0] == '\0' || !field.getter || !field.setter) {
            return false;
        }
    }
    return true;
}

constexpr bool sameText(const char *left, const char *right) {
    while (*left && *right) {
        if (*left++ != *right++)
            return false;
    }
    return *left == *right;
}

constexpr bool hasUniqueMappingKeys() {
    for (std::size_t left = 0; left < Descriptors.size(); ++left) {
        for (std::size_t right = left + 1; right < Descriptors.size(); ++right) {
            if (Descriptors[left].mappingIndex == Descriptors[right].mappingIndex &&
                sameText(Descriptors[left].mappingName, Descriptors[right].mappingName)) {
                return false;
            }
        }
    }
    return true;
}

static_assert(Descriptors.size() == StateField::Count,
              "Every StateField ID requires exactly one descriptor");
static_assert(hasCompleteDescriptors(), "StateField descriptors must be complete");
static_assert(hasUniqueMappingKeys(), "StateField mapping name/index pairs must be unique");
} // namespace
const StateField::Descriptor *StateField::descriptor(int id) noexcept {
    return id >= 0 && id < Count ? &Descriptors[std::size_t(id)] : nullptr;
}
const StateField::Registry &StateField::all() noexcept {
    return Descriptors;
}

int StateField::resolve(const QString &name, int index) {
    if (index < 0)
        return -1;
    for (std::size_t id = 0; id < Descriptors.size(); ++id) {
        const Descriptor &candidate = Descriptors[id];
        if (candidate.mappingIndex == index && name == QLatin1String(candidate.mappingName)) {
            return int(id);
        }
    }
    return -1;
}

QString StateField::displayName(int id) {
    const Descriptor *field = descriptor(id);
    return field ? QString::fromLatin1(field->memberName) : QString();
}

QString StateField::presentationName(int id) {
    const Descriptor *field = descriptor(id);
    return field ? QString::fromLatin1(field->presentationName) : QString();
}

std::optional<double> StateField::tryValue(const Plane &plane, const Target &target,
                                           int id) noexcept {
    const Descriptor *field = id >= Location0 && id <= Time ? descriptor(id) : nullptr;
    if (!field)
        return std::nullopt;
    static const dlz::TelemetryInputs EmptyDlzInputs{};
    return field->getter(plane, target, EmptyDlzInputs);
}

std::optional<double> StateField::tryValue(const DecodedState &state, int id) noexcept {
    const Descriptor *field = descriptor(id);
    if (!field)
        return std::nullopt;
    return field->getter(state.plane, state.target, state.dlzInputs);
}
double StateField::value(const Plane &plane, const Target &target, int id) {
    return tryValue(plane, target, id).value_or(std::numeric_limits<double>::quiet_NaN());
}
double StateField::value(const DecodedState &state, int id) {
    return tryValue(state, id).value_or(std::numeric_limits<double>::quiet_NaN());
}

bool StateField::setValue(Plane *plane, Target *target, int id, double value) {
    const Descriptor *field = id >= Location0 && id <= Time ? descriptor(id) : nullptr;
    if (!plane || !target || !field)
        return false;
    dlz::TelemetryInputs ignored;
    field->setter(*plane, *target, ignored, value);
    return true;
}

bool StateField::setValue(DecodedState *state, int id, double value) {
    const Descriptor *field = descriptor(id);
    if (!state || !field)
        return false;
    field->setter(state->plane, state->target, state->dlzInputs, value);
    return true;
}
