
#include "viewer/plane/plane_attitude_transform.h"

#include "domain/statefield.h"

#include <QtMath>

#include <cmath>

namespace {

bool available(const QBitArray &fields, int field) noexcept {
    return field >= 0 && field < fields.size() && fields.testBit(field);
}

double finiteAngle(const Plane &plane, int index, const QBitArray &fields, int field) noexcept {
    constexpr double TwoPi = 6.28318530717958647692;
    return available(fields, field) && std::isfinite(plane.euler[index])
               ? std::remainder(plane.euler[index], TwoPi)
               : 0.0;
}

} // namespace

QQuaternion PlaneAttitudeTransform::orientation(const Plane &plane,
                                                const QBitArray &availableFields,
                                                bool *complete) noexcept {
    const bool hasYaw =
        available(availableFields, StateField::Euler0) && std::isfinite(plane.euler[0]);
    const bool hasPitch =
        available(availableFields, StateField::Euler1) && std::isfinite(plane.euler[1]);
    const bool hasRoll =
        available(availableFields, StateField::Euler2) && std::isfinite(plane.euler[2]);
    if (complete != nullptr) {
        *complete = hasYaw && hasPitch && hasRoll;
    }

    const float yawDegrees = static_cast<float>(
        qRadiansToDegrees(finiteAngle(plane, 0, availableFields, StateField::Euler0)));
    const float pitchDegrees = static_cast<float>(
        qRadiansToDegrees(finiteAngle(plane, 1, availableFields, StateField::Euler1)));
    const float rollDegrees = static_cast<float>(
        qRadiansToDegrees(finiteAngle(plane, 2, availableFields, StateField::Euler2)));

    const QQuaternion yaw = QQuaternion::fromAxisAndAngle(0.0F, 1.0F, 0.0F, -yawDegrees);
    const QQuaternion pitch = QQuaternion::fromAxisAndAngle(1.0F, 0.0F, 0.0F, pitchDegrees);
    const QQuaternion roll = QQuaternion::fromAxisAndAngle(0.0F, 0.0F, -1.0F, rollDegrees);
    const QQuaternion orientation = (yaw * pitch * roll).normalized();
    if (!std::isfinite(orientation.scalar()) || !std::isfinite(orientation.x()) ||
        !std::isfinite(orientation.y()) || !std::isfinite(orientation.z())) {
        return {};
    }
    return orientation;
}
