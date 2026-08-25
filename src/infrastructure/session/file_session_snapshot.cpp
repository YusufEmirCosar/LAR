
#include "infrastructure/session/file_session_snapshot.h"

#include <QFile>
#include <QIODevice>

namespace {
constexpr qint64 CopyChunkSize = 4 * 1024 * 1024;
}

bool FileSessionSnapshot::writeTo(QIODevice &destination, QString *error) const {
    if (m_sourcePath.isEmpty() || m_byteCount < 0 || !destination.isOpen() ||
        !destination.isWritable()) {
        if (error)
            *error = QStringLiteral("Session snapshot is invalid");
        return false;
    }

    QFile source(m_sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (error)
            *error = source.errorString();
        return false;
    }

    qint64 remaining = m_byteCount;
    while (remaining > 0) {
        const QByteArray chunk = source.read(qMin(CopyChunkSize, remaining));
        if (chunk.isEmpty()) {
            if (error) {
                *error = source.errorString().isEmpty()
                             ? QStringLiteral("Session source ended before its snapshot boundary")
                             : source.errorString();
            }
            return false;
        }
        if (destination.write(chunk) != chunk.size()) {
            if (error)
                *error = destination.errorString();
            return false;
        }
        remaining -= chunk.size();
    }
    return true;
}
