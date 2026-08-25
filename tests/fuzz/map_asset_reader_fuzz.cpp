#include "viewer/map/map_asset_reader.h"

#include <QByteArrayView>

#include <cstddef>
#include <cstdint>
#include <limits>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    if (size > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
        return 0;
    }
    const QByteArrayView bytes(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
    (void)lar::map::MapAssetReader::read(bytes);
    return 0;
}
