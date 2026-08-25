#include "geojson_source_reader.h"

#include <QByteArray>

#include <cstddef>
#include <cstdint>
#include <limits>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    if (size > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
        return 0;
    }
    const QByteArray bytes(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
    (void)lar::map::tool::GeoJsonSourceReader::parse(bytes);
    return 0;
}
