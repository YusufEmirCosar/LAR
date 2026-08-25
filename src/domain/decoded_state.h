#pragma once

/**
 * @file decoded_state.h
 * @brief Atomic decoded Plane/Target/DLZ state published by runtime paths.
 */

#include "domain/dlz/dlz_types.h"
#include "domain/state.h"

#include <QBitArray>
#include <QMetaType>

/** @brief One validated mapped frame and its field-availability mask. */
struct DecodedState final {
    Plane plane{};                    ///< Legacy aircraft state.
    Target target{};                  ///< Legacy target and LAR state.
    dlz::TelemetryInputs dlzInputs{}; ///< Optional atomic DLZ telemetry triple.
    QBitArray availableFields;        ///< StateField IDs present in this frame.
};

Q_DECLARE_METATYPE(DecodedState)
