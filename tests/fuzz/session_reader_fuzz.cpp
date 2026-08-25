#include "infrastructure/session/lar_session_reader.h"

#include <QByteArray>

#include <cstddef>
#include <cstdint>
#include <limits>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    constexpr std::size_t MaximumInput = 32U * 1024U * 1024U;
    if (size > MaximumInput ||
        size > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max())) {
        return 0;
    }
    const QByteArray bytes(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
    LarSessionReader reader;
    QString error;
    if (!reader.loadData(bytes, &error))
        return 0;
    SessionTimestamp timestamp;
    SessionStateItem item;
    if (reader.recordCount() > 0) {
        (void)reader.timestampAt(0, &timestamp, &error);
        (void)reader.recordAt(0, &item, &error);
        const qint64 last = reader.recordCount() - 1;
        (void)reader.timestampAt(last, &timestamp, &error);
        (void)reader.recordAt(last, &item, &error);
    }
    return 0;
}
