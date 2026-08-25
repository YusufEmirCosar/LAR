#pragma once

/**
 * @file grid_lar_view.h
 * @brief Painter-based local Cartesian LAR viewport page.
 */

#include "viewer/grid_geometry_builder.h"
#include "viewer/lar_geometry_builder.h"
#include "viewer/viewport/grid_camera_transform.h"
#include "viewer/viewport/lar_viewport_page.h"

#include <QPixmap>
#include <QWidget>

#include <initializer_list>

class QPainter;

/** @brief Draws grid, LAR geometry, navigation cues, and entity markers. */
class GridLarView final : public QWidget, public ILarViewportPage {
    Q_OBJECT

  public:
    explicit GridLarView(QWidget *parent = nullptr);

    QWidget &widget() noexcept override;
    LarViewportPageEvents &events() noexcept override;
    void setSceneState(const LarSceneState &state) override;
    void clearScene() override;
    void setCameraState(const ViewportCameraState &state) override;
    void fitToData() override;

  protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

  private:
    bool hasFields(std::initializer_list<int> fields) const;
    QPointF geographicToCameraWorld(const double position[3]) const;
    QPointF worldToScreen(const QPointF &world) const;
    QPointF screenToWorld(const QPointF &screen) const;
    double effectiveCameraBearing() const;
    void updateTrackedCamera();
    void beginFreeMovement();
    void panFreeCamera(const QPoint &pixelDelta);

    void drawGrid(QPainter &painter) const;
    void drawInRange(QPainter &painter) const;
    void drawInZone(QPainter &painter) const;
    void drawMarkers(QPainter &painter) const;
    void drawNavigation(QPainter &painter) const;

    LarSceneState m_scene;
    ViewportCameraState m_cameraState;
    LarViewportPageEvents m_events;
    GridCameraTransform m_cameraTransform;
    QPixmap m_navigator;
    QPoint m_lastMousePosition;
    bool m_dragging = false;
};
