#pragma once

/**
 * @file lar_marker_layer.h
 * @brief Transparent painter overlay for Earth-view entities and diagnostics.
 */

#include "domain/state.h"

#include <QBitArray>
#include <QPointF>
#include <QString>
#include <QWidget>

#include <functional>
#include <initializer_list>

/** @brief Immutable state snapshot consumed for one overlay paint. */
struct LarMarkerState final {
    Plane plane{};
    Target target{};
    QBitArray availableFields;
    QString message;
    bool hasScene = false;
    double cameraBearingDegrees = 0.0;
};

/** @brief Projects and paints plane/target markers over an OpenGL map. */
class LarMarkerLayer final : public QWidget {
    Q_OBJECT

  public:
    using Projector = std::function<bool(double latitudeRadians, double longitudeRadians,
                                         QPointF &screen, bool &visible)>;
    using StateSource = std::function<LarMarkerState()>;

    explicit LarMarkerLayer(Projector projector, StateSource stateSource,
                            QWidget *parent = nullptr);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    bool hasFields(const LarMarkerState &state, std::initializer_list<int> fields) const;
    QPointF projectedMarker(double latitudeRadians, double longitudeRadians, bool &visible) const;
    double projectedPlaneHeadingDegrees(const LarMarkerState &state,
                                        const QPointF &planeScreen) const;
    void drawTarget(QPainter &painter, const LarMarkerState &state) const;
    void drawPlane(QPainter &painter, const LarMarkerState &state) const;

    Projector m_projector;
    StateSource m_stateSource;
};
