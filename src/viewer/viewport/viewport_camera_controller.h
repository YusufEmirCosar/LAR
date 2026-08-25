#pragma once

/**
 * @file viewport_camera_controller.h
 * @brief Shared tracking policy independent of any viewport renderer.
 */

#include "domain/state.h"
#include "viewer/lar_camera.h"

#include <QBitArray>

#include <array>
#include <initializer_list>

/** @brief Renderer-neutral camera anchor and bearing snapshot. */
struct ViewportCameraState final {
    CameraTrackingMode mode = CameraTrackingMode::FollowPlane;
    bool turnWithPlane = true;
    bool trackingActive = false;
    bool hasAnchor = false;
    std::array<double, 3> anchorRadians{};
    double bearingRadians = 0.0;
};

/** @brief Selects stable plane/target anchors from the available state fields. */
class ViewportCameraController final {
  public:
    void setScene(const Plane &plane, const Target &target, const QBitArray &availableFields);
    void clearScene() noexcept;
    void setMode(CameraTrackingMode mode) noexcept;
    void setTurnWithPlane(bool enabled) noexcept;

    [[nodiscard]] CameraTrackingMode mode() const noexcept;
    [[nodiscard]] bool turnWithPlane() const noexcept;
    [[nodiscard]] const ViewportCameraState &state() const noexcept;

  private:
    [[nodiscard]] bool hasFields(std::initializer_list<int> fields) const;
    void refresh() noexcept;

    Plane m_plane{};
    Target m_target{};
    QBitArray m_availableFields;
    ViewportCameraState m_state;
    bool m_hasScene = false;
};
