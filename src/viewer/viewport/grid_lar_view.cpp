
#include "viewer/viewport/grid_lar_view.h"

#include "domain/statefield.h"
#include "viewer/lar_geodesic_geometry.h"
#include "viewer/viewport/lar_zone_input_validator.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr double EarthRadiusMeters = 6371008.8;
constexpr double Pi = 3.14159265358979323846;

} // namespace

GridLarView::GridLarView(QWidget *parent)
    : QWidget(parent), m_navigator(QStringLiteral(":/icons/navigator.png")) {
    setFocusPolicy(Qt::StrongFocus);
}

QWidget &GridLarView::widget() noexcept {
    return *this;
}

LarViewportPageEvents &GridLarView::events() noexcept {
    return m_events;
}

void GridLarView::setSceneState(const LarSceneState &state) {
    const auto fieldChanged = [this, &state](int field) {
        const bool wasAvailable = m_scene.hasScene && field >= 0 &&
                                  field < m_scene.availableFields.size() &&
                                  m_scene.availableFields.testBit(field);
        const bool isAvailable = state.hasScene && field >= 0 &&
                                 field < state.availableFields.size() &&
                                 state.availableFields.testBit(field);
        return wasAvailable != isAvailable ||
               (isAvailable && StateField::value(m_scene.plane, m_scene.target, field) !=
                                   StateField::value(state.plane, state.target, field));
    };
    static constexpr std::array<int, 12> VisibleFields{
        StateField::Location0, StateField::Location1, StateField::Euler0, StateField::IrPos0,
        StateField::IrPos1,    StateField::IrR,       StateField::IzPos0, StateField::IzPos1,
        StateField::IzTheta1,  StateField::IzTheta2,  StateField::IzR1,   StateField::IzR2};
    const bool visibleStateChanged =
        m_scene.hasScene != state.hasScene ||
        std::any_of(VisibleFields.begin(), VisibleFields.end(), fieldChanged);
    m_scene = state;
    if (!m_cameraTransform.hasOrigin() &&
        hasFields({StateField::Location0, StateField::Location1})) {
        m_cameraTransform.setOrigin(m_scene.plane.location[0], m_scene.plane.location[1]);
    }
    updateTrackedCamera();
    if (visibleStateChanged) {
        update();
    }
}

void GridLarView::clearScene() {
    m_scene = {};
    m_cameraTransform.clear();
    m_dragging = false;
    update();
}

void GridLarView::setCameraState(const ViewportCameraState &state) {
    if (m_cameraState.mode == state.mode && m_cameraState.turnWithPlane == state.turnWithPlane &&
        m_cameraState.trackingActive == state.trackingActive &&
        m_cameraState.hasAnchor == state.hasAnchor &&
        m_cameraState.anchorRadians == state.anchorRadians &&
        m_cameraState.bearingRadians == state.bearingRadians) {

        return;
    }
    m_cameraState = state;
    updateTrackedCamera();
    update();
}

bool GridLarView::hasFields(std::initializer_list<int> fields) const {
    return std::all_of(fields.begin(), fields.end(), [this](int field) {
        return field >= 0 && field < m_scene.availableFields.size() &&
               m_scene.availableFields.testBit(field);
    });
}

double GridLarView::effectiveCameraBearing() const {
    return m_cameraTransform.bearingRadians();
}

void GridLarView::updateTrackedCamera() {
    m_cameraTransform.applyCameraState(m_cameraState);
}

QPointF GridLarView::geographicToCameraWorld(const double position[3]) const {
    return m_cameraTransform.geographicToWorld(position);
}

QPointF GridLarView::worldToScreen(const QPointF &world) const {
    return m_cameraTransform.worldToScreen(world, size());
}

QPointF GridLarView::screenToWorld(const QPointF &screen) const {
    return m_cameraTransform.screenToWorld(screen, size());
}

void GridLarView::fitToData() {
    if (!m_scene.hasScene || width() <= 0 || height() <= 0) {
        return;
    }
    updateTrackedCamera();
    const LarZoneValidationResult validation =
        LarZoneInputValidator().validate(m_scene.target, m_scene.availableFields);
    double horizontalExtent = 1000.0;
    double verticalExtent = 1000.0;
    if (m_cameraTransform.hasLocation() && validation.inRange) {

        const QPointF center = geographicToCameraWorld(m_scene.target.ir_pos);
        horizontalExtent =
            qMax(horizontalExtent, qAbs(center.x()) + validation.inRange->outerRadiusMeters);
        verticalExtent =
            qMax(verticalExtent, qAbs(center.y()) + validation.inRange->outerRadiusMeters);
    }
    if (m_cameraTransform.hasLocation() && validation.inZone) {

        const QPointF center = geographicToCameraWorld(m_scene.target.iz_pos);
        horizontalExtent =
            qMax(horizontalExtent, qAbs(center.x()) + validation.inZone->outerRadiusMeters);
        verticalExtent =
            qMax(verticalExtent, qAbs(center.y()) + validation.inZone->outerRadiusMeters);
    }
    if (m_cameraTransform.hasLocation() &&
        hasFields({StateField::Location0, StateField::Location1})) {

        const QPointF plane = geographicToCameraWorld(m_scene.plane.location);
        horizontalExtent = qMax(horizontalExtent, qAbs(plane.x()) + 50.0);
        verticalExtent = qMax(verticalExtent, qAbs(plane.y()) + 50.0);
    }
    m_cameraTransform.setScale(
        std::clamp(std::min((width() * 0.5 - 45.0) / qMax(1.0, horizontalExtent),
                            (height() * 0.5 - 45.0) / qMax(1.0, verticalExtent)),
                   0.00001, 50.0));
    update();
}

void GridLarView::paintEvent(QPaintEvent *) {
    emit m_events.frameRendered();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(QStringLiteral("#f4f5f2")));
    drawGrid(painter);
    if (!m_scene.hasScene) {
        painter.setPen(QColor(QStringLiteral("#65706a")));
        painter.drawText(rect().adjusted(30, 30, -30, -30), Qt::AlignCenter,
                         QStringLiteral("WAITING FOR LAR DATA"));
    } else {
        drawInRange(painter);
        drawInZone(painter);
        drawMarkers(painter);
    }
    drawNavigation(painter);
}

void GridLarView::drawGrid(QPainter &painter) const {
    const double targetWorldSpacing = 90.0 / m_cameraTransform.scale();
    painter.setPen(QPen(QColor(QStringLiteral("#d9ddda")), 1));

    if (!m_scene.hasScene || !m_cameraTransform.hasLocation() || !m_cameraTransform.hasOrigin()) {

        const double step = GridGeometryBuilder::gridStep(targetWorldSpacing);
        const QPointF topLeft = screenToWorld(QPointF(0, 0));
        const QPointF bottomRight = screenToWorld(QPointF(width(), height()));
        for (double x = std::floor(topLeft.x() / step) * step; x <= bottomRight.x(); x += step) {
            const double screenX = worldToScreen(QPointF(x, 0)).x();
            painter.drawLine(QPointF(screenX, 0), QPointF(screenX, height()));
        }
        for (double y = std::floor(bottomRight.y() / step) * step; y <= topLeft.y(); y += step) {
            const double screenY = worldToScreen(QPointF(0, y)).y();
            painter.drawLine(QPointF(0, screenY), QPointF(width(), screenY));
        }
        return;
    }

    const double bearing = effectiveCameraBearing();
    const double cosineBearing = std::cos(bearing);
    const double sineBearing = std::sin(bearing);
    const QPointF corners[] = {screenToWorld(QPointF(0, 0)), screenToWorld(QPointF(width(), 0)),
                               screenToWorld(QPointF(width(), height())),
                               screenToWorld(QPointF(0, height()))};

    const auto groundOffset = [cosineBearing, sineBearing](const QPointF &world) {
        return QPointF(world.x() * cosineBearing + world.y() * sineBearing,
                       -world.x() * sineBearing + world.y() * cosineBearing);
    };
    const QPointF firstGroundCorner = groundOffset(corners[0]);
    double minimumEast = firstGroundCorner.x();
    double maximumEast = firstGroundCorner.x();
    double minimumNorth = firstGroundCorner.y();
    double maximumNorth = firstGroundCorner.y();
    for (int index = 1; index < 4; ++index) {
        const QPointF ground = groundOffset(corners[index]);
        minimumEast = qMin(minimumEast, ground.x());
        maximumEast = qMax(maximumEast, ground.x());
        minimumNorth = qMin(minimumNorth, ground.y());
        maximumNorth = qMax(maximumNorth, ground.y());
    }

    const double spacingMeters = GridGeometryBuilder::gridStep(targetWorldSpacing);
    const double originCosineLatitude = qMax(0.01, std::cos(m_cameraTransform.originLatitude()));
    const auto groundCoordinates = [this, originCosineLatitude](double latitude, double longitude) {
        double longitudeDelta = longitude - m_cameraTransform.originLongitude();
        while (longitudeDelta > Pi) {
            longitudeDelta -= 2.0 * Pi;
        }
        while (longitudeDelta < -Pi) {
            longitudeDelta += 2.0 * Pi;
        }
        return QPointF(EarthRadiusMeters * longitudeDelta * originCosineLatitude,
                       EarthRadiusMeters * (latitude - m_cameraTransform.originLatitude()));
    };
    const auto &cameraLocation = m_cameraTransform.location();
    const QPointF cameraGround = groundCoordinates(cameraLocation[0], cameraLocation[1]);
    const double minimumGroundEast = cameraGround.x() + minimumEast;
    const double maximumGroundEast = cameraGround.x() + maximumEast;
    const double minimumGroundNorth = cameraGround.y() + minimumNorth;
    const double maximumGroundNorth = cameraGround.y() + maximumNorth;
    const auto groundToScreen = [this, cosineBearing, sineBearing, cameraGround](double east,
                                                                                 double north) {
        const double relativeEast = east - cameraGround.x();
        const double relativeNorth = north - cameraGround.y();
        return worldToScreen(QPointF(relativeEast * cosineBearing - relativeNorth * sineBearing,
                                     relativeEast * sineBearing + relativeNorth * cosineBearing));
    };

    const qint64 firstNorthIndex = qint64(std::floor(minimumGroundNorth / spacingMeters));
    const qint64 lastNorthIndex = qint64(std::ceil(maximumGroundNorth / spacingMeters));
    for (qint64 index = firstNorthIndex; index <= lastNorthIndex; ++index) {
        const double north = static_cast<double>(index) * spacingMeters;
        painter.drawLine(groundToScreen(minimumGroundEast, north),
                         groundToScreen(maximumGroundEast, north));
    }
    const qint64 firstEastIndex = qint64(std::floor(minimumGroundEast / spacingMeters));
    const qint64 lastEastIndex = qint64(std::ceil(maximumGroundEast / spacingMeters));
    for (qint64 index = firstEastIndex; index <= lastEastIndex; ++index) {
        const double east = static_cast<double>(index) * spacingMeters;
        painter.drawLine(groundToScreen(east, minimumGroundNorth),
                         groundToScreen(east, maximumGroundNorth));
    }
}

void GridLarView::drawInRange(QPainter &painter) const {
    const LarZoneValidationResult validation =
        LarZoneInputValidator().validate(m_scene.target, m_scene.availableFields);
    if (!m_cameraTransform.hasLocation() || !validation.inRange) {

        return;
    }
    const QPointF center = worldToScreen(geographicToCameraWorld(m_scene.target.ir_pos));
    const double radius = validation.inRange->outerRadiusMeters * m_cameraTransform.scale();
    painter.setPen(QPen(QColor(QStringLiteral("#346d91")), 2));
    painter.setBrush(QColor(52, 109, 145, 28));
    painter.drawEllipse(center, radius, radius);
}

void GridLarView::drawInZone(QPainter &painter) const {
    const LarZoneValidationResult validation =
        LarZoneInputValidator().validate(m_scene.target, m_scene.availableFields);
    if (!m_cameraTransform.hasLocation() || !validation.inZone) {

        return;
    }

    const QPointF center = geographicToCameraWorld(m_scene.target.iz_pos);
    const LarZoneDefinition &zone = *validation.inZone;
    const QPolygonF worldPolygon = LarGeometryBuilder::inZoneWedgePolygon(
        center, zone.innerRadiusMeters, zone.outerRadiusMeters, zone.startBearingRadians,
        zone.startBearingRadians + zone.spanRadians, effectiveCameraBearing());
    QPolygonF screenPolygon;
    screenPolygon.reserve(worldPolygon.size());
    for (const QPointF &point : worldPolygon) {
        screenPolygon.append(worldToScreen(point));
    }
    QPainterPath path;
    path.addPolygon(screenPolygon);
    path.closeSubpath();
    painter.fillPath(path, QColor(20, 131, 102, 76));
    painter.setPen(QPen(QColor(QStringLiteral("#0e755b")), 2));
    painter.drawPath(path);
}

void GridLarView::drawMarkers(QPainter &painter) const {
    if (m_cameraTransform.hasLocation() && hasFields({StateField::IzPos0, StateField::IzPos1})) {
        const QPointF targetPosition =
            worldToScreen(geographicToCameraWorld(m_scene.target.iz_pos));
        painter.setPen(QPen(QColor(QStringLiteral("#9e3f2f")), 2));
        painter.setBrush(QColor(QStringLiteral("#d85849")));
        const QPolygonF targetShape{targetPosition + QPointF(0, -10),
                                    targetPosition + QPointF(8.66, 5),
                                    targetPosition + QPointF(-8.66, 5)};
        painter.drawPolygon(targetShape);
    }
    if (m_cameraTransform.hasLocation() &&
        hasFields({StateField::Location0, StateField::Location1})) {
        const QPointF planePosition =
            worldToScreen(geographicToCameraWorld(m_scene.plane.location));
        painter.save();
        painter.translate(planePosition);
        const double relativeHeading =
            (hasFields({StateField::Euler0}) ? m_scene.plane.euler[0] : 0.0) -
            effectiveCameraBearing();
        painter.rotate(std::remainder(relativeHeading, LarGeodesicGeometry::TwoPi) * 180.0 / Pi);
        const QPolygonF planeShape{QPointF(0, -15), QPointF(9, 11), QPointF(0, 7), QPointF(-9, 11)};
        painter.setPen(QPen(QColor(QStringLiteral("#173f54")), 2));
        painter.setBrush(QColor(QStringLiteral("#3f87a6")));
        painter.drawPolygon(planeShape);
        painter.restore();
    }
}

void GridLarView::drawNavigation(QPainter &painter) const {
    constexpr double NavigatorSize = 25.0;
    constexpr double BottomMargin = 16.0;
    constexpr double ScaleMaximumWidth = 110.0;
    constexpr double NavigatorGap = 16.0;
    constexpr double HorizontalPadding = 14.0;
    constexpr double VerticalPadding = 10.0;

    const double distance =
        GridGeometryBuilder::scaleBarDistance(ScaleMaximumWidth / m_cameraTransform.scale());
    const double scaleWidth = distance * m_cameraTransform.scale();
    const double contentWidth = ScaleMaximumWidth + NavigatorGap + NavigatorSize;
    const QSizeF boxSize(contentWidth + HorizontalPadding * 2.0,
                         NavigatorSize + VerticalPadding * 2.0);
    const QRectF box((width() - boxSize.width()) * 0.5, height() - BottomMargin - boxSize.height(),
                     boxSize.width(), boxSize.height());
    const QRectF navigatorRect(box.right() - HorizontalPadding - NavigatorSize,
                               box.top() + VerticalPadding, NavigatorSize, NavigatorSize);
    const double scaleSlotLeft = box.left() + HorizontalPadding;
    const double scaleSlotRight = navigatorRect.left() - NavigatorGap;
    const double scaleCenter = (scaleSlotLeft + scaleSlotRight) * 0.5;
    const double scaleLeft = scaleCenter - scaleWidth * 0.5;
    const double scaleRight = scaleCenter + scaleWidth * 0.5;
    const double scaleY = box.center().y() + 9.0;
    const double bearing = m_scene.hasScene ? effectiveCameraBearing() : 0.0;

    painter.save();
    painter.setPen(QPen(QColor(QStringLiteral("#101814")), 1.5));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(box, 8.0, 8.0);
    painter.restore();

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.translate(navigatorRect.center());
    painter.rotate(-bearing * 180.0 / Pi);
    painter.drawPixmap(
        QRectF(-NavigatorSize * 0.5, -NavigatorSize * 0.5, NavigatorSize, NavigatorSize),
        m_navigator, QRectF(m_navigator.rect()));
    painter.restore();

    painter.save();
    QFont labelFont = painter.font();
    labelFont.setPixelSize(11);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.setPen(QColor(QStringLiteral("#28352f")));
    painter.drawText(QRectF(scaleLeft - 8.0, scaleY - 24.0, scaleWidth + 16.0, 16.0),
                     Qt::AlignCenter, GridGeometryBuilder::formatDistance(distance));
    painter.setPen(QPen(QColor(QStringLiteral("#28352f")), 2));
    painter.drawLine(QPointF(scaleLeft, scaleY), QPointF(scaleRight, scaleY));
    painter.drawLine(QPointF(scaleLeft, scaleY - 6.0), QPointF(scaleLeft, scaleY + 1.0));
    painter.drawLine(QPointF(scaleRight, scaleY - 6.0), QPointF(scaleRight, scaleY + 1.0));
    painter.restore();
}

void GridLarView::wheelEvent(QWheelEvent *event) {
    m_cameraTransform.zoom(event->angleDelta().y());
    update();
    event->accept();
}

void GridLarView::mouseDoubleClickEvent(QMouseEvent *event) {
    fitToData();
    event->accept();
}

void GridLarView::beginFreeMovement() {
    if (m_cameraState.mode != CameraTrackingMode::Free) {
        emit m_events.freeMovementRequested();
    }
}

void GridLarView::panFreeCamera(const QPoint &pixelDelta) {
    m_cameraTransform.pan(pixelDelta);
    update();
}

void GridLarView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        beginFreeMovement();
        m_dragging = true;
        m_lastMousePosition = event->pos();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void GridLarView::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->pos() - m_lastMousePosition;
        m_lastMousePosition = event->pos();
        panFreeCamera(delta);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void GridLarView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
