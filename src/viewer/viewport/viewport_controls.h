#pragma once

/**
 * @file viewport_controls.h
 * @brief UI controls for view projection and camera tracking policy.
 */

#include "viewer/lar_camera.h"

#include <QBitArray>
#include <QGroupBox>

#include "domain/state.h"

class QCheckBox;
class QComboBox;
class QEvent;
class QLabel;
class QPushButton;

/** @brief Presents viewport choices and emits typed user requests. */
class ViewportControls final : public QGroupBox {
    Q_OBJECT

  public:
    explicit ViewportControls(QWidget *parent = nullptr);

    LarViewMode viewMode() const;
    CameraTrackingMode cameraTrackingMode() const;
    bool turnWithPlane() const;

    void setViewMode(LarViewMode mode);
    void setCameraTrackingMode(CameraTrackingMode mode);
    void setTurnWithPlane(bool enabled);
    void setTurnWithPlaneAvailability(bool yawAvailable);
    void setNavigationReadout(const Plane &plane, const Target &target,
                              const QBitArray &availableFields);

  signals:
    void viewModeRequested(LarViewMode mode);
    void cameraTrackingModeRequested(CameraTrackingMode mode);
    void turnWithPlaneRequested(bool enabled);

  private:
    void updateTurnWithPlanePresentation(bool yawAvailable);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    QComboBox *m_viewModeSelector = nullptr;
    QComboBox *m_cameraTrackingSelector = nullptr;
    QCheckBox *m_turnWithPlaneCheckBox = nullptr;
    QPushButton *m_gridButton = nullptr;
    QPushButton *m_mercatorButton = nullptr;
    QPushButton *m_sphereButton = nullptr;
    QPushButton *m_planeButton = nullptr;
    QPushButton *m_targetButton = nullptr;
    QPushButton *m_freeButton = nullptr;
    QLabel *m_navigationReadout = nullptr;
    bool m_yawAvailable = true;
};
