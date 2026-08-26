#pragma once

/**
 * @file dlz_hud_view.h
 * @brief Widget that owns the current render-ready DLZ HUD frame.
 */

#include "domain/dlz/dlz_types.h"
#include "viewer/hud/dlz_hud_renderer.h"

#include <QWidget>

class QPaintEvent;

namespace dlz::presentation {

/** @brief Lightweight QWidget that paints the latest immutable DLZ frame. */
class HudView final : public QWidget {
    Q_OBJECT

  public:
    explicit HudView(QWidget *parent = nullptr);

    void setFrame(const dlz::Solution &solution, const dlz::HudState &hudState,
                  double displayedRangeNm);
    void clearFrame();

    void setError(const QString &message);

    const dlz::HudState &hudState() const noexcept {
        return m_hudState;
    }
    double displayedRangeNm() const noexcept {
        return m_displayedRangeNm;
    }
    /** True while the view owns a valid render frame. */
    bool hasFrame() const noexcept {
        return m_hasFrame;
    }
    /** True while a persistent canvas error is shown. */
    bool hasError() const noexcept {
        return !m_errorText.isEmpty();
    }
    const QString &errorText() const noexcept {
        return m_errorText;
    }

  signals:
    void frameRendered();

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    HudRenderer m_renderer;
    dlz::Solution m_solution;
    dlz::HudState m_hudState;
    double m_displayedRangeNm = 0.0;
    bool m_hasFrame = false;
    QString m_errorText;
};

} // namespace dlz::presentation
