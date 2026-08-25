#pragma once

/**
 * @file statevalueformatter.h
 * @brief Unit-aware formatting for scalar Plane and Target fields.
 */

#include <QString>

/** Presentation-only conversion of a state scalar to a user-facing value. */
class StateValueFormatter {
  public:
    static QString format(int fieldId, double value);
};
