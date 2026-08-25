#pragma once

/**
 * @file map_palette.h
 * @brief Centralized normalized RGBA colors for world-map rendering.
 */

#include <QVector4D>

namespace lar::map::palette {

inline const QVector4D Land{153.0F / 255.0F, 153.0F / 255.0F, 153.0F / 255.0F, 1.0F};

inline const QVector4D Border{210.0F / 255.0F, 210.0F / 255.0F, 210.0F / 255.0F, 1.0F};

inline const QVector4D Ocean{228.0F / 255.0F, 228.0F / 255.0F, 228.0F / 255.0F, 1.0F};

inline const QVector4D Space{23.0F / 255.0F, 25.0F / 255.0F, 28.0F / 255.0F, 1.0F};

} // namespace lar::map::palette
