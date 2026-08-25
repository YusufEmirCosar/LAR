#pragma once

/**
 * @file plane_view_workspace.h
 * @brief Hosted Plane page with lower-corner upload and display controls.
 */

#include "viewer/viewport/lar_viewport_page.h"

#include <QWidget>

class QFrame;
class QGroupBox;
class QLabel;
class PlaneSceneWidget;
class QPushButton;

/** @brief Third top-level viewport content page for the centered Plane simulation. */
class PlaneViewWorkspace final : public QWidget, public ILarViewportPage {
    Q_OBJECT

  public:
    explicit PlaneViewWorkspace(QString packageDirectory, QWidget *parent = nullptr);

    QWidget &widget() noexcept override {
        return *this;
    }
    LarViewportPageEvents &events() noexcept override {
        return m_events;
    }
    void setSceneState(const LarSceneState &state) override;
    void clearScene() override;
    void setCameraState(const ViewportCameraState &state) override;
    void fitToData() override;

    [[nodiscard]] PlaneSceneWidget *sceneWidget() const noexcept {
        return m_sceneWidget;
    }

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

  private:
    void forwardEvent(QEvent &event);
    void refreshTerrainControls();

    PlaneSceneWidget *m_sceneWidget = nullptr;
    QGroupBox *m_uploadPanel = nullptr;
    QFrame *m_displayPanel = nullptr;
    QPushButton *m_uploadModelButton = nullptr;
    QPushButton *m_uploadTerrainButton = nullptr;
    QPushButton *m_terrainButton = nullptr;
    QPushButton *m_surfaceButton = nullptr;
    QPushButton *m_changeSkyboxButton = nullptr;
    QLabel *m_openGlFallback = nullptr;
    LarViewportPageEvents m_events;
};
