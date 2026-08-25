#pragma once

/**
 * @file lar_camera.h
 * @brief Shared viewport presentation and tracking mode enumerations.
 */

#include <QMetaType>

/** @brief Available spatial presentations for LAR state. */
enum class LarViewMode { Grid = 0, Mercator = 1, Sphere = 2 };

/** @brief Determines which entity anchors the viewport camera. */
enum class CameraTrackingMode { FollowPlane = 0, FollowTarget = 1, Free = 2 };

Q_DECLARE_METATYPE(LarViewMode)
Q_DECLARE_METATYPE(CameraTrackingMode)
