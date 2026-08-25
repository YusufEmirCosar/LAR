#pragma once

/**
 * @file lar_scene_state.h
 * @brief Complete immutable-style scene value distributed to viewport pages.
 */

#include "domain/state.h"

#include <QBitArray>

/** @brief Plane, target, field availability, and scene-presence flag. */
struct LarSceneState final {
    Plane plane{};
    Target target{};
    QBitArray availableFields;
    bool hasScene = false;
};
