#pragma once

/**
 * @file lar_navigation_overlay.h
 * @brief Shared scale-bar and north-indicator overlay for Earth pages.
 */

#include <QPixmap>
#include <QWidget>

#include <functional>

/** @brief Paints the same scale/navigation box used by the Grid page. */
class LarNavigationOverlay final : public QWidget {
    Q_OBJECT

  public:
    using MetersPerPixelSource = std::function<double()>;
    using BearingRadiansSource = std::function<double()>;

    explicit LarNavigationOverlay(MetersPerPixelSource metersPerPixelSource,
                                  BearingRadiansSource bearingRadiansSource,
                                  QWidget *parent = nullptr);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    MetersPerPixelSource m_metersPerPixelSource;
    BearingRadiansSource m_bearingRadiansSource;
    QPixmap m_navigator;
};
