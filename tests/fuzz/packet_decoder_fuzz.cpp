#include "infrastructure/mapping/json_mapping_repository.h"
#include "infrastructure/mapping/mapped_packet_decoder.h"

#include <QByteArray>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    constexpr std::size_t MaximumInput = 1024U * 1024U;
    if (size < 2U || size > MaximumInput ||
        size > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
        return 0;
    }
    const std::size_t split = 1U + (static_cast<std::size_t>(data[0]) * (size - 1U) / 255U);
    const QByteArray mappingJson(reinterpret_cast<const char *>(data + 1U),
                                 static_cast<qsizetype>(split - 1U));
    const QByteArray packet(reinterpret_cast<const char *>(data + split),
                            static_cast<qsizetype>(size - split));

    JsonMappingRepository repository;
    PacketMapping mapping;
    if (!repository.loadJson(mappingJson, &mapping))
        return 0;
    MappedPacketDecoder decoder(std::move(mapping));
    DecodedState state;
    QString error;
    (void)decoder.decode(packet, &state, &error);
    return 0;
}
