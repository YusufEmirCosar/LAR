#pragma once

/**
 * @file packet_decoder.h
 * @brief Port for decoding mapped binary packets into domain state.
 */

#include "domain/decoded_state.h"
#include "domain/packet_mapping.h"

#include <QBitArray>
#include <QByteArray>
#include <QString>

/** @brief Mutable decoder whose active PacketMapping can be replaced. */
class IPacketDecoder {
  public:
    virtual ~IPacketDecoder() = default;

    virtual void setMapping(PacketMapping mapping) = 0;

    virtual bool decode(const QByteArray &packet, DecodedState *state,
                        QString *error = nullptr) const = 0;
};
