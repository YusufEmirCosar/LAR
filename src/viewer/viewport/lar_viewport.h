#pragma once

/**
 * @file lar_viewport.h
 * @brief Host widget that switches pages and presents state only on the active page.
 */

#include "domain/dlz/dlz_types.h"
#include "viewer/lar_camera.h"
#include "viewer/viewport/lar_scene_state.h"
#include "viewer/viewport/lar_viewport_page.h"
#include "viewer/viewport/viewport_camera_controller.h"
#include "viewer/viewport/viewport_content_mode.h"
#include "viewer/viewport_frame_counter.h"

#include <QBitArray>
#include <QString>
#include <QWidget>

#include <memory>

class GridLarView;
class ViewportContentSwitch;
class QResizeEvent;

/**
 * @brief Stable viewport boundary for Grid, Mercator, and Sphere modes.
 *
 * The host owns the concrete pages, retains the latest shared scene, updates
 * only the visible presentation page, and guards input forwarding against
 * ignored-event propagation loops. A newly selected page is synchronized
 * before it becomes visible.
 */
class LarViewport final : public QWidget {
    Q_OBJECT

  public:
    explicit LarViewport(QWidget *parent = nullptr);
    LarViewport(std::unique_ptr<ILarViewportPage> gridPage,
                std::unique_ptr<IEarthLarViewportPage> earthPage,
                std::unique_ptr<QWidget> hudPage = nullptr, QWidget *parent = nullptr);
    LarViewport(std::unique_ptr<ILarViewportPage> gridPage,
                std::unique_ptr<IEarthLarViewportPage> earthPage,
                std::unique_ptr<ILarViewportPage> planePage, std::unique_ptr<QWidget> hudPage,
                QWidget *parent = nullptr);

    LarViewMode viewMode() const noexcept {
        return m_viewMode;
    }
    CameraTrackingMode cameraTrackingMode() const noexcept {
        return m_cameraController.mode();
    }
    bool turnWithPlane() const noexcept {
        return m_cameraController.turnWithPlane();
    }
    ViewportContentMode contentMode() const noexcept {
        return m_contentMode;
    }
    ViewportContentSwitch *contentSwitch() const noexcept {
        return m_contentSwitch;
    }

  public slots:
    void setState(const Plane &plane, const Target &target, const QBitArray &availableFields);
    void setDlzInputs(const dlz::TelemetryInputs &inputs, bool available,
                      const QString &source = {});

    void clearState();
    void fitToData();
    void resetTotalFrameCount();
    void setViewMode(LarViewMode mode);
    void setContentMode(ViewportContentMode mode);
    void setOverlayWidget(QWidget *overlay) noexcept;
    void setCameraTrackingMode(CameraTrackingMode mode);
    void setTurnWithPlane(bool enabled);

  signals:
    void framesPerSecondChanged(int framesPerSecond);
    void totalFrameCountChanged(quint64 totalFrames);
    void viewModeChanged(LarViewMode mode);
    void contentModeChanged(ViewportContentMode mode);
    void cameraTrackingModeChanged(CameraTrackingMode mode);
    void turnWithPlaneChanged(bool enabled);
    void diagnosticRaised(const QString &message);

  protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    ILarViewportPage &activePage() noexcept;
    ILarViewportPage *activeScenePage() noexcept;
    QWidget &activePageWidget() noexcept;
    void showActivePage();
    void applyCameraStateToActivePage();
    void applySceneStateToActivePage();
    void applyDlzInputsToActivePage();
    void synchronizeActivePage();
    void raiseOverlayWidget() noexcept;
    void forwardEvent(QEvent &event);

    LarSceneState m_scene;
    dlz::TelemetryInputs m_dlzInputs;
    QString m_dlzSource;
    ViewportFrameCounter m_frameCounter;
    ILarViewportPage *m_gridPage = nullptr;
    IEarthLarViewportPage *m_earthPage = nullptr;
    ILarViewportPage *m_planePage = nullptr;
    QWidget *m_hudPage = nullptr;
    ViewportContentSwitch *m_contentSwitch = nullptr;
    LarViewMode m_viewMode = LarViewMode::Grid;
    ViewportContentMode m_contentMode = ViewportContentMode::Lar;
    ViewportCameraController m_cameraController;
    QWidget *m_overlayWidget = nullptr;
    bool m_hasInitialFit = false;
    bool m_hasReceivedSceneState = false;
    bool m_dlzInputsAvailable = false;
    bool m_hasReceivedDlzInputs = false;
    bool m_forwardingInputEvent = false;
};
