#pragma once

/**
 * @file plane_attitude_transform.h
 * @brief Pure conversion from protocol Euler angles to the 3D aircraft orientation.
 */

#include "domain/state.h"

#include <QBitArray>
#include <QQuaternion>

/** @brief Builds the world-space attitude for a model whose nose points along -Z. */
class PlaneAttitudeTransform final {
  public:
    [[nodiscard]] static QQuaternion orientation(const Plane &plane,
                                                 const QBitArray &availableFields,
                                                 bool *complete = nullptr) noexcept;
};
