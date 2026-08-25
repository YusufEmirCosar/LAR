
#include "viewer/hud/dlz_hud_view.h"

#include <QPainter>

namespace dlz::presentation {

HudView::HudView(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("dlzHudView"));
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(300, 300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void HudView::setFrame(const dlz::Solution &solution, const dlz::HudState &hudState,
                       double displayedRangeNm) {
    m_solution = solution;
    m_hudState = hudState;
    m_displayedRangeNm = displayedRangeNm;
    m_hasFrame = true;
    m_errorText.clear();
    update();
}

void HudView::clearFrame() {
    m_hasFrame = false;
    m_errorText.clear();
    update();
}

void HudView::setError(const QString &message) {
    m_hasFrame = false;
    m_errorText = message.trimmed();
    update();
}

void HudView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    if (!m_errorText.isEmpty()) {
        m_renderer.drawError(painter, rect(), m_errorText);
    } else if (m_hasFrame) {
        m_renderer.draw(painter, rect(), m_solution, m_hudState, m_displayedRangeNm);
    } else {
        m_renderer.drawNoTrack(painter, rect());
    }
    emit frameRendered();
}

} // namespace dlz::presentation
