#pragma once

/**
 * @file lar_viewport_page.h
 * @brief Substitutable viewport-page interfaces and their shared event channel.
 */

#include "viewer/viewport/lar_scene_state.h"
#include "viewer/viewport/viewport_camera_controller.h"

#include <QObject>

class QWidget;

/** @brief QObject signal bundle exposed by non-QObject page interfaces. */
class LarViewportPageEvents final : public QObject {
    Q_OBJECT

  signals:
    void frameRendered();
    void freeMovementRequested();
    void diagnosticRaised(const QString &message);
    void pageUnavailable();
};

/** @brief Common contract implemented by every viewport presentation page. */
class ILarViewportPage {
  public:
    virtual ~ILarViewportPage() = default;

    virtual QWidget &widget() noexcept = 0;
    virtual LarViewportPageEvents &events() noexcept = 0;
    virtual void setSceneState(const LarSceneState &state) = 0;
    virtual void clearScene() = 0;
    virtual void setCameraState(const ViewportCameraState &state) = 0;
    virtual void fitToData() = 0;
};

/** @brief Extra availability and mode operations required by Earth pages. */
class IEarthLarViewportPage : public ILarViewportPage {
  public:
    virtual bool ensureAvailable() = 0;
    virtual void setEarthViewMode(LarViewMode mode) = 0;
};
