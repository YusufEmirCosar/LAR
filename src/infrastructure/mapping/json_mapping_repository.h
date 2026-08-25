#pragma once

/**
 * @file json_mapping_repository.h
 * @brief JSON implementation of the packet-mapping repository port.
 */

#include "application/ports/mapping_repository.h"

/** @brief Parses mapping JSON and constructs validated PacketMapping values. */
class JsonMappingRepository final : public IMappingRepository {
  public:
    bool loadFile(const QString &path, PacketMapping *mapping, QString *error = nullptr) override;
    bool loadJson(const QByteArray &json, PacketMapping *mapping,
                  QString *error = nullptr) override;
};
