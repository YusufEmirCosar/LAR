#pragma once

/**
 * @file mapped_packet_decoder.h
 * @brief Packet decoder adapter backed by the domain PacketMapping.
 */

#include "application/ports/packet_decoder.h"
#include "domain/packet_mapping.h"

/** @brief Thin IPacketDecoder adapter around PacketMapping::decode(). */
class MappedPacketDecoder final : public IPacketDecoder {
  public:
    MappedPacketDecoder() = default;
    explicit MappedPacketDecoder(PacketMapping mapping) : m_mapping(std::move(mapping)) {}

    void setMapping(PacketMapping mapping) override {
        m_mapping = std::move(mapping);
    }
    const PacketMapping &mapping() const {
        return m_mapping;
    }

    bool decode(const QByteArray &packet, DecodedState *state,
                QString *error = nullptr) const override {
        return m_mapping.decode(packet, state, error);
    }

    bool decode(const QByteArray &packet, Plane *plane, Target *target, QBitArray *availableFields,
                QString *error = nullptr) const {
        if (!plane || !target || !availableFields) {
            if (error)
                *error = QStringLiteral("Decoder output objects are required");
            return false;
        }
        DecodedState state;
        if (!m_mapping.decode(packet, &state, error))
            return false;
        *plane = state.plane;
        *target = state.target;
        *availableFields = state.availableFields;
        return true;
    }

  private:
    PacketMapping m_mapping;
};
