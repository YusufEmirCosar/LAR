#pragma once

/**
 * @file dlz_hud_renderer.h
 * @brief Stateless painter for launch-zone symbology and diagnostic states.
 */

#include "domain/dlz/dlz_types.h"

#include <QRectF>
#include <QString>

class QPainter;

namespace dlz::presentation {

/** @brief Draws only the DLZ range staple and its required readouts. */
class HudRenderer final {
  public:
    void draw(QPainter &painter, const QRectF &bounds, const dlz::Solution &solution,
              const dlz::HudState &hudState, double displayedRangeNm) const;

    void drawNoTrack(QPainter &painter, const QRectF &bounds) const;

    void drawError(QPainter &painter, const QRectF &bounds, const QString &message) const;
};

} // namespace dlz::presentation
