
#include "viewer/viewport/lar_navigation_overlay.h"

#include "viewer/grid_geometry_builder.h"

#include <QPaintEvent>
#include <QPainter>

#include <cmath>
#include <utility>

namespace {
constexpr double Pi = 3.14159265358979323846;
constexpr double NavigatorSize = 25.0;
constexpr double BottomMargin = 16.0;
constexpr double ScaleMaximumWidth = 110.0;
constexpr double NavigatorGap = 16.0;
constexpr double HorizontalPadding = 14.0;
constexpr double VerticalPadding = 10.0;
} // namespace

LarNavigationOverlay::LarNavigationOverlay(MetersPerPixelSource metersPerPixelSource,
                                           BearingRadiansSource bearingRadiansSource,
                                           QWidget *parent)
    : QWidget(parent), m_metersPerPixelSource(std::move(metersPerPixelSource)),
      m_bearingRadiansSource(std::move(bearingRadiansSource)),
      m_navigator(QStringLiteral(":/icons/navigator.png")) {
    setObjectName(QStringLiteral("larNavigationOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
}

void LarNavigationOverlay::paintEvent(QPaintEvent *) {
    if (!m_metersPerPixelSource || width() <= 0 || height() <= 0) {
        return;
    }

    const double metersPerPixel = m_metersPerPixelSource();
    if (!std::isfinite(metersPerPixel) || metersPerPixel <= 0.0) {
        return;
    }
    const double distance =
        GridGeometryBuilder::scaleBarDistance(ScaleMaximumWidth * metersPerPixel);
    if (!std::isfinite(distance) || distance <= 0.0) {
        return;
    }

    const double scaleWidth = distance / metersPerPixel;
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
    const double bearing = m_bearingRadiansSource ? m_bearingRadiansSource() : 0.0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

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
