#include "viewer/help/help_context.h"

#include "viewer/viewport/lar_viewport.h"

#include <QWidget>

namespace lar::help {

QString currentTopic(const QWidget *focused, const QWidget *onlinePanel,
                     const QWidget *offlinePanel, const LarViewport *viewport, bool offlineMode) {
    if (focused != nullptr && offlinePanel != nullptr &&
        (focused == offlinePanel || offlinePanel->isAncestorOf(focused))) {
        return QStringLiteral("offline-replay");
    }
    if (focused != nullptr && onlinePanel != nullptr &&
        (focused == onlinePanel || onlinePanel->isAncestorOf(focused))) {
        return QStringLiteral("online-capture");
    }

    const bool viewportFocused = focused != nullptr && viewport != nullptr &&
                                 (focused == viewport || viewport->isAncestorOf(focused));
    if (viewport != nullptr &&
        (viewportFocused || viewport->contentMode() != ViewportContentMode::Lar)) {
        switch (viewport->contentMode()) {
        case ViewportContentMode::Plane:
            return QStringLiteral("plane-view");
        case ViewportContentMode::Hud:
            return QStringLiteral("dlz-view");
        case ViewportContentMode::Lar:
            return QStringLiteral("lar-views");
        }
    }
    return offlineMode ? QStringLiteral("offline-replay") : QStringLiteral("online-capture");
}

} // namespace lar::help
