#pragma once

/**
 * @file viewport_content_mode.h
 * @brief Top-level selection between geographic LAR, Plane, and DLZ content.
 */

/** @brief Top-level content selection, independent from LAR projection. */
enum class ViewportContentMode { Lar = 0, Plane = 1, Hud = 2 };
