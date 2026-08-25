#pragma once

/**
 * @file packet_mapping.h
 * @brief Immutable mapping between packet byte ranges and domain state fields.
 */

#include "domain/state.h"
#include "domain/statefield.h"

#include "domain/decoded_state.h"

#include <QBitArray>
#include <QByteArray>
#include <QString>
#include <QVector>

/** @brief One scalar field's byte offset and IEEE-754 storage width. */
struct FieldBinding {
    int fieldId = -1; ///< Resolved StateField::Id.
    int offset = 0;   ///< Zero-based byte offset in a datagram.
    int size = 0;     ///< IEEE-754 storage width: four or eight bytes.
};

/**
 * @brief Validated packet schema that decodes and encodes Plane/Target values.
 *
 * A mapping owns the original JSON so a recording can embed the exact schema
 * needed to decode its packets during later playback.
 */
class PacketMapping final {
  public:
    PacketMapping() = default;
    PacketMapping(QVector<FieldBinding> bindings, QByteArray json);

    /**
     * @brief Returns whether all bindings form a complete non-overlapping schema.
     *
     * @return True when the reported condition holds; false otherwise.
     */
    bool isValid() const noexcept {
        return m_valid;
    }
    const QVector<FieldBinding> &bindings() const noexcept {
        return m_bindings;
    }
    int minimumPacketSize() const noexcept {
        return m_minimumPacketSize;
    }
    const QBitArray &availableFields() const noexcept {
        return m_availableFields;
    }
    const QByteArray &json() const noexcept {
        return m_json;
    }

    bool decode(const QByteArray &packet, Plane *plane, Target *target, QBitArray *availableFields,
                QString *error = nullptr) const;
    bool decode(const QByteArray &packet, DecodedState *state, QString *error = nullptr) const;
    QByteArray encode(const Plane &plane, const Target &target) const;
    QByteArray encode(const DecodedState &state) const;

  private:
    QVector<FieldBinding> m_bindings;
    QBitArray m_availableFields{StateField::Count};
    QByteArray m_json;
    int m_minimumPacketSize = 0;
    bool m_valid = false;
};
