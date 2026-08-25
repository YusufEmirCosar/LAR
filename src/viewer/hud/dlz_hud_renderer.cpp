
#include "viewer/hud/dlz_hud_renderer.h"

#include "viewer/hud/dlz_range_scale.h"

#include <QBrush>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace dlz::presentation {
namespace {

const QColor HudGreen(QStringLiteral("#79ff9d"));
const QColor HudGreenDim(QStringLiteral("#3ea86b"));
const QColor NezAmber(QStringLiteral("#d8e95e"));
const QColor HudBlack(QStringLiteral("#050806"));

QString rangeText(double rangeNm) {
    return QStringLiteral("%1 NM").arg(rangeNm, 0, 'f', 1);
}

void drawTick(QPainter &painter, double x, double y, double length, const QColor &color,
              double width) {
    painter.setPen(QPen(color, width, Qt::SolidLine, Qt::SquareCap));
    painter.drawLine(QPointF(x, y), QPointF(x + length, y));
}

} // namespace

void HudRenderer::drawNoTrack(QPainter &painter, const QRectF &bounds) const {
    painter.save();
    painter.fillRect(bounds, HudBlack);
    painter.setPen(QPen(HudGreenDim, 1.0));
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(std::max(12, static_cast<int>(bounds.height() * 0.035)));
    painter.setFont(font);
    painter.drawText(bounds, Qt::AlignCenter, QStringLiteral("NO TRACK"));
    painter.restore();
}

void HudRenderer::drawError(QPainter &painter, const QRectF &bounds, const QString &message) const {
    painter.save();
    painter.fillRect(bounds, HudBlack);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(std::max(12, static_cast<int>(bounds.height() * 0.032)));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(NezAmber);
    const QRectF textBounds = bounds.adjusted(
        std::max(20.0, bounds.width() * 0.10), std::max(20.0, bounds.height() * 0.10),
        -std::max(20.0, bounds.width() * 0.10), -std::max(20.0, bounds.height() * 0.10));
    painter.drawText(textBounds, Qt::AlignCenter | Qt::TextWordWrap,
                     message.trimmed().isEmpty() ? QStringLiteral("DLZ input is invalid")
                                                 : message.trimmed());
    painter.restore();
}

void HudRenderer::draw(QPainter &painter, const QRectF &bounds, const dlz::Solution &solution,
                       const dlz::HudState &hudState, double displayedRangeNm) const {
    const bool finiteInputs =
        hudState.mode == dlz::HudMode::Prelaunch &&
        std::isfinite(solution.aerodynamicMaximumRangeNm) &&
        std::isfinite(solution.interceptRangeNm) && std::isfinite(solution.noEscapeRangeNm) &&
        std::isfinite(solution.turnAndRunRangeNm) && std::isfinite(solution.minimumRangeNm) &&
        std::isfinite(solution.timeOfFlightSeconds) && std::isfinite(hudState.currentRangeNm) &&
        std::isfinite(hudState.rangeRateKnots) && std::isfinite(hudState.scaleMinimumNm) &&
        std::isfinite(hudState.scaleMaximumNm) &&
        std::isfinite(hudState.timeToImpactRemainingSeconds) && std::isfinite(displayedRangeNm) &&
        hudState.scaleMaximumNm > hudState.scaleMinimumNm;
    if (!finiteInputs) {
        drawNoTrack(painter, bounds);
        return;
    }
    painter.save();
    painter.fillRect(bounds, HudBlack);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const double marginX = std::max(20.0, bounds.width() * 0.08);
    const double top = bounds.top() + std::max(28.0, bounds.height() * 0.10);
    const double bottom = bounds.bottom() - std::max(28.0, bounds.height() * 0.10);
    // Keep the primary DLZ scale centered in the HUD canvas.  The range labels
    // are laid out to its right, while the current-range caret stays adjacent
    // to the scale so it reads as a marker on that same axis.
    const double scaleX = bounds.center().x();
    const double stapleWidth = std::max(18.0, std::min(42.0, bounds.width() * 0.075));
    const double tickLength = std::max(16.0, std::min(32.0, bounds.width() * 0.055));
    const double lineWidth = std::max(1.0, bounds.width() / 700.0);
    const double caretX = scaleX - std::max(3.0, lineWidth * 2.0);
    const auto yFor = [&](double range) {
        return rangeToY(range, hudState.scaleMinimumNm, hudState.scaleMaximumNm, top, bottom);
    };

    const double yAero = yFor(solution.aerodynamicMaximumRangeNm);
    const double yPi = yFor(solution.interceptRangeNm);
    const double yNe = yFor(solution.noEscapeRangeNm);
    const double yMin = yFor(solution.minimumRangeNm);
    const double yCaret = yFor(displayedRangeNm);

    painter.setPen(QPen(HudGreen, lineWidth, Qt::SolidLine, Qt::SquareCap));
    painter.drawLine(QPointF(scaleX, top), QPointF(scaleX, bottom));

    painter.setPen(QPen(HudGreen, lineWidth, Qt::SolidLine, Qt::SquareCap));
    painter.drawLine(QPointF(scaleX, yAero), QPointF(scaleX + stapleWidth, yAero));
    painter.drawLine(QPointF(scaleX + stapleWidth, yAero), QPointF(scaleX + stapleWidth, yMin));
    painter.drawLine(QPointF(scaleX + stapleWidth, yMin), QPointF(scaleX, yMin));

    const double bandTop = std::min(yNe, yMin);
    const double bandBottom = std::max(yNe, yMin);
    painter.fillRect(QRectF(scaleX - lineWidth * 0.5, bandTop, stapleWidth + lineWidth,
                            std::max(0.0, bandBottom - bandTop)),
                     QBrush(QColor(NezAmber.red(), NezAmber.green(), NezAmber.blue(), 70)));
    painter.setPen(QPen(NezAmber, lineWidth, Qt::DashLine, Qt::SquareCap));
    for (double y = bandTop + 4.0; y < bandBottom; y += 8.0) {
        painter.drawLine(QPointF(scaleX, y), QPointF(scaleX + stapleWidth, y));
    }

    drawTick(painter, scaleX, yAero, tickLength, HudGreen, lineWidth);
    drawTick(painter, scaleX, yPi, tickLength, HudGreen, lineWidth);
    drawTick(painter, scaleX, yNe, tickLength, NezAmber, lineWidth);
    drawTick(painter, scaleX, yMin, tickLength, HudGreen, lineWidth);

    QPainterPath caret;
    caret.moveTo(caretX, yCaret);
    caret.lineTo(caretX - std::max(7.0, tickLength * 0.40),
                 yCaret - std::max(5.0, tickLength * 0.28));
    caret.lineTo(caretX - std::max(7.0, tickLength * 0.40),
                 yCaret + std::max(5.0, tickLength * 0.28));
    caret.closeSubpath();
    painter.setPen(QPen(HudGreen, lineWidth, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(HudGreen);
    painter.drawPath(caret);

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(std::max(10, static_cast<int>(bounds.height() * 0.028)));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(HudGreen);
    const double currentRangeTextWidth = std::max(70.0, bounds.width() * 0.26);
    // Keep the numeric marker visibly separate from the caret's left edge.
    const double currentRangeTextGap = std::max(12.0, tickLength * 0.50);
    painter.drawText(QRectF(caretX - currentRangeTextWidth - currentRangeTextGap,
                            yCaret - font.pixelSize() * 0.8, currentRangeTextWidth,
                            font.pixelSize() * 1.6),
                     Qt::AlignRight | Qt::AlignVCenter, rangeText(displayedRangeNm));

    QFont labelFont = font;
    labelFont.setPixelSize(std::max(9, static_cast<int>(bounds.height() * 0.022)));
    painter.setFont(labelFont);
    painter.setPen(HudGreenDim);
    const auto label = [&](double y, const QString &name, double value) {
        painter.drawText(
            QRectF(scaleX + stapleWidth + 8.0, y - labelFont.pixelSize() * 0.75,
                   bounds.right() - scaleX - stapleWidth - 10.0, labelFont.pixelSize() * 1.5),
            Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("%1 %2").arg(name, rangeText(value)));
    };
    label(yAero, QStringLiteral("RMAX"), solution.aerodynamicMaximumRangeNm);
    label(yPi, QStringLiteral("RPI"), solution.interceptRangeNm);
    painter.setPen(NezAmber);
    label(yNe, QStringLiteral("RNE"), solution.noEscapeRangeNm);
    label(yMin, QStringLiteral("RMIN"), solution.minimumRangeNm);

    painter.setPen(HudGreenDim);
    painter.drawText(QRectF(scaleX - 20.0, bottom + labelFont.pixelSize() * 0.5, 80.0,
                            labelFont.pixelSize() * 1.5),
                     Qt::AlignLeft,
                     QStringLiteral("0 / %1 NM").arg(hudState.scaleMaximumNm, 0, 'f', 0));

    if (hudState.shootFlash) {
        painter.setPen(QPen(HudGreen, lineWidth * 1.4, Qt::SolidLine, Qt::SquareCap));
        painter.drawText(QRectF(bounds.left() + marginX, top - labelFont.pixelSize() * 1.8,
                                bounds.width() * 0.38, labelFont.pixelSize() * 1.6),
                         Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("SHOOT"));
    }
    painter.restore();
}

} // namespace dlz::presentation
