#pragma once

/**
 * @file statevalidator.h
 * @brief Domain validation for mapped Plane and Target values.
 */

#include "domain/state.h"

#include "domain/decoded_state.h"

#include <QBitArray>
#include <QString>

/**
 * @brief Checks finite values and semantic ranges for available state fields.
 */
class StateValidator {
  public:
    static bool validate(const Plane &plane, const Target &target, const QBitArray &availableFields,
                         QString *error = nullptr);
    static bool validate(const DecodedState &state, QString *error = nullptr);
};
