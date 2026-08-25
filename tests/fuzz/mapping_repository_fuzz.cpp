#include "infrastructure/mapping/json_mapping_repository.h"

#include <QByteArray>

#include <cstddef>
#include <cstdint>
#include <limits>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    constexpr std::size_t MaximumInput = 16U * 1024U * 1024U;
    if (size == 0U || size > MaximumInput ||
        size > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
        return 0;
    }
    const QByteArray bytes(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
    JsonMappingRepository repository;
    PacketMapping mapping;
    QString error;
    (void)repository.loadJson(bytes, &mapping, &error);
    return 0;
}
