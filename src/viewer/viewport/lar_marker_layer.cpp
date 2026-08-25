
#include "viewer/viewport/lar_marker_layer.h"

#include "domain/statefield.h"
#include "viewer/lar_geodesic_geometry.h"

#include <QPaintEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {

constexpr double RadiansToDegrees = 180.0 / LarGeodesicGeometry::Pi;

} // namespace

LarMarkerLayer::LarMarkerLayer(Projector projector, StateSource stateSource, QWidget *parent)
    : QWidget(parent), m_projector(std::move(projector)), m_stateSource(std::move(stateSource)) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
}

bool LarMarkerLayer::hasFields(const LarMarkerState &state,
                               std::initializer_list<int> fields) const {
    return std::all_of(fields.begin(), fields.end(), [&state](int field) {
        return field >= 0 && field < state.availableFields.size() &&
               state.availableFields.testBit(field);
    });
}

QPointF LarMarkerLayer::projectedMarker(double latitudeRadians, double longitudeRadians,
                                        bool &visible) const {
    QPointF screen;
    if (!m_projector || !m_projector(latitudeRadians, longitudeRadians, screen, visible)) {
        visible = false;
    }
    return screen;
}

double LarMarkerLayer::projectedPlaneHeadingDegrees(const LarMarkerState &state,
                                                    const QPointF &planeScreen) const {
    if (!hasFields(state, {StateField::Location0, StateField::Location1, StateField::Euler0})) {
        return 0.0;
    }
    const GeoCoordinateRadians plane{state.plane.location[0], state.plane.location[1]};
    const GeoCoordinateRadians ahead =
        LarGeodesicGeometry::destination(plane, 1000.0, state.plane.euler[0]);
    bool visible = false;
    const QPointF aheadScreen = projectedMarker(ahead.latitude, ahead.longitude, visible);
    const double dx = aheadScreen.x() - planeScreen.x();
    const double dy = aheadScreen.y() - planeScreen.y();
    if (!std::isfinite(dx) || !std::isfinite(dy) || std::hypot(dx, dy) < 0.01) {
        return state.plane.euler[0] * RadiansToDegrees - state.cameraBearingDegrees;
    }
    return std::atan2(dx, -dy) * RadiansToDegrees;
}

void LarMarkerLayer::drawTarget(QPainter &painter, const LarMarkerState &state) const {
    if (!hasFields(state, {StateField::IzPos0, StateField::IzPos1})) {
        return;
    }
    bool visible = false;
    const QPointF position =
        projectedMarker(state.target.iz_pos[0], state.target.iz_pos[1], visible);
    if (!visible) {
        return;
    }

    painter.setPen(QPen(QColor(QStringLiteral("#9e3f2f")), 2));
    painter.setBrush(QColor(QStringLiteral("#d85849")));
    const QPolygonF shape{position + QPointF(0, -10), position + QPointF(8.66, 5),
                          position + QPointF(-8.66, 5)};
    painter.drawPolygon(shape);
}

void LarMarkerLayer::drawPlane(QPainter &painter, const LarMarkerState &state) const {
    if (!hasFields(state, {StateField::Location0, StateField::Location1})) {
        return;
    }
    bool visible = false;
    const QPointF position =
        projectedMarker(state.plane.location[0], state.plane.location[1], visible);
    if (!visible) {
        return;
    }

    painter.save();
    painter.translate(position);
    painter.rotate(projectedPlaneHeadingDegrees(state, position));
    const QPolygonF shape{QPointF(0, -15), QPointF(9, 11), QPointF(0, 7), QPointF(-9, 11)};
    painter.setPen(QPen(QColor(QStringLiteral("#173f54")), 2));
    painter.setBrush(QColor(QStringLiteral("#3f87a6")));
    painter.drawPolygon(shape);
    painter.restore();
}

void LarMarkerLayer::paintEvent(QPaintEvent *) {
    if (!m_stateSource) {
        return;
    }
    const LarMarkerState state = m_stateSource();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!state.message.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#7a2d23")));
        painter.setBrush(QColor(255, 255, 255, 224));
        painter.drawText(QRectF(rect()).adjusted(28, 28, -28, -28),
                         Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, state.message);
    }

    if (!state.hasScene) {
        painter.setPen(QColor(QStringLiteral("#65706a")));
        painter.drawText(rect().adjusted(30, 30, -30, -30), Qt::AlignCenter,
                         QStringLiteral("WAITING FOR LAR DATA"));
        return;
    }

    drawTarget(painter, state);
    drawPlane(painter, state);
}
