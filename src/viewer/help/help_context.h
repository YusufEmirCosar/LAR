#pragma once

/**
 * @file help_context.h
 * @brief Context-to-help-topic selection for the main viewer window.
 */

#include <QString>

class LarViewport;
class QWidget;

namespace lar::help {

/**
 * @brief Returns the help topic that best matches the focused control and active viewer state.
 */
[[nodiscard]] QString currentTopic(const QWidget *focused, const QWidget *onlinePanel,
                                   const QWidget *offlinePanel, const LarViewport *viewport,
                                   bool offlineMode);

} // namespace lar::help
