
#include "viewer/viewport/viewport_camera_controller.h"

#include "domain/statefield.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double Pi = 3.14159265358979323846;
constexpr double TwoPi = 6.28318530717958647692;

bool validAnchor(const double position[3]) noexcept {
    return std::isfinite(position[0]) && std::isfinite(position[1]) && position[0] >= -Pi * 0.5 &&
           position[0] <= Pi * 0.5 && position[1] >= -Pi && position[1] <= Pi;
}
} // namespace

void ViewportCameraController::setScene(const Plane &plane, const Target &target,
                                        const QBitArray &availableFields) {
    m_plane = plane;
    m_target = target;
    m_availableFields = availableFields;
    m_hasScene = true;
    refresh();
}

void ViewportCameraController::clearScene() noexcept {
    m_hasScene = false;
    m_availableFields.clear();
    m_state.trackingActive = false;
    m_state.bearingRadians = 0.0;
}

void ViewportCameraController::setMode(CameraTrackingMode mode) noexcept {
    m_state.mode = mode;
    refresh();
}

void ViewportCameraController::setTurnWithPlane(bool enabled) noexcept {
    m_state.turnWithPlane = enabled;
    refresh();
}

CameraTrackingMode ViewportCameraController::mode() const noexcept {
    return m_state.mode;
}

bool ViewportCameraController::turnWithPlane() const noexcept {
    return m_state.turnWithPlane;
}

const ViewportCameraState &ViewportCameraController::state() const noexcept {
    return m_state;
}

bool ViewportCameraController::hasFields(std::initializer_list<int> fields) const {
    return std::all_of(fields.begin(), fields.end(), [this](int field) {
        return field >= 0 && field < m_availableFields.size() && m_availableFields.testBit(field);
    });
}

void ViewportCameraController::refresh() noexcept {
    m_state.trackingActive = m_hasScene && m_state.mode != CameraTrackingMode::Free;
    if (!m_hasScene) {
        m_state.bearingRadians = 0.0;
        return;
    }

    const double *candidate = nullptr;
    if (m_state.mode == CameraTrackingMode::FollowPlane &&
        hasFields({StateField::Location0, StateField::Location1}) &&
        validAnchor(m_plane.location)) {

        candidate = m_plane.location;
    } else if (m_state.mode == CameraTrackingMode::FollowTarget &&
               hasFields({StateField::IzPos0, StateField::IzPos1}) &&
               validAnchor(m_target.iz_pos)) {

        candidate = m_target.iz_pos;
    }

    if (candidate != nullptr) {
        std::copy_n(candidate, 3, m_state.anchorRadians.begin());
        m_state.hasAnchor = true;
    } else if (!m_state.hasAnchor) {
        if (hasFields({StateField::Location0, StateField::Location1}) &&
            validAnchor(m_plane.location)) {

            std::copy_n(m_plane.location, 3, m_state.anchorRadians.begin());

            m_state.hasAnchor = true;
        } else if (hasFields({StateField::IzPos0, StateField::IzPos1}) &&
                   validAnchor(m_target.iz_pos)) {

            std::copy_n(m_target.iz_pos, 3, m_state.anchorRadians.begin());

            m_state.hasAnchor = true;
        }
    }

    m_state.bearingRadians = 0.0;
    if (m_state.mode == CameraTrackingMode::FollowPlane && m_state.turnWithPlane &&
        hasFields({StateField::Euler0}) && std::isfinite(m_plane.euler[0])) {

        m_state.bearingRadians = std::remainder(m_plane.euler[0], TwoPi);
    }
}
