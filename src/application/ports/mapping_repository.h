#pragma once

/**
 * @file mapping_repository.h
 * @brief Port for loading and validating packet-mapping definitions.
 */

#include "domain/packet_mapping.h"
#include <QString>

/** @brief Creates PacketMapping values from files or JSON bytes. */
class IMappingRepository {
  public:
    virtual ~IMappingRepository() = default;

    virtual bool loadFile(const QString &path, PacketMapping *mapping,
                          QString *error = nullptr) = 0;
    virtual bool loadJson(const QByteArray &json, PacketMapping *mapping,
                          QString *error = nullptr) = 0;
};
