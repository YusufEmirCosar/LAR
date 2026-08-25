
#include "infrastructure/session/lar_session_writer.h"
#include "application/session_limits.h"
#include "infrastructure/mapping/json_mapping_repository.h"
#include "infrastructure/session/file_session_snapshot.h"

#include <QDataStream>

#include <algorithm>
#include <limits>

namespace {
void configureStream(QDataStream &stream) {
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setVersion(QDataStream::Qt_6_5);
}
} // namespace

LarSessionWriter::~LarSessionWriter() {
    cancel();
}

bool LarSessionWriter::begin(const QByteArray &mappingJson, QString *error) {
    if (isActive()) {
        if (error)
            *error = QStringLiteral("A session writer transaction is already active");
        return false;
    }
    return openSession(mappingJson, error);
}

bool LarSessionWriter::append(SessionTimestamp timestamp, const QByteArray &packet,
                              QString *error) {
    if (!m_file) {
        if (error)
            *error = QStringLiteral("Session writer is not open");
        return false;
    }
    if (packet.isEmpty() || packet.size() > int(lar::session::MaximumPacketSize)) {
        if (error)
            *error = QStringLiteral("Validated UDP packet has an unsupported size");
        return false;
    }
    DecodedState state;
    QString packetError;
    if (!m_mapping.decode(packet, &state, &packetError)) {
        if (error)
            *error = QStringLiteral("Cannot record invalid UDP package: %1").arg(packetError);
        return false;
    }

    const quint64 timestampMs = static_cast<quint64>(timestamp.milliseconds());
    if (m_recordCount > 0 && timestampMs < m_lastTimestampMs) {
        if (error)
            *error = QStringLiteral("Session package time is not monotonic");
        return false;
    }

    QByteArray encoded;
    encoded.reserve(int(sizeof(quint64) + sizeof(quint32)) + packet.size());
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    configureStream(stream);
    stream << timestampMs << quint32(packet.size());
    if (stream.writeRawData(packet.constData(), packet.size()) != packet.size() ||
        stream.status() != QDataStream::Ok) {

        if (error)
            *error = QStringLiteral("Cannot encode UDP package record");
        return false;
    }

    const qint64 previousSize = m_file->size();
    if (!m_file->seek(previousSize) || m_file->write(encoded) != encoded.size()) {
        const QString writeError = m_file->errorString();
        const bool rolledBack = m_file->resize(previousSize) && m_file->seek(previousSize);
        if (!rolledBack) {
            const QString rollbackError = m_file->errorString();
            cancel();
            if (error) {
                *error = QStringLiteral("Session append failed (%1) and rollback failed (%2); "
                                        "the recording transaction was discarded")
                             .arg(writeError, rollbackError);
            }
            return false;
        }
        if (error)
            *error = writeError;
        return false;
    }
    m_lastTimestampMs = timestampMs;
    ++m_recordCount;
    return true;
}

bool LarSessionWriter::reset(QString *error) {
    if (!m_file || m_mappingJson.isEmpty()) {
        if (error)
            *error = QStringLiteral("No recording session is active");
        return false;
    }
    return openSession(m_mappingJson, error);
}

void LarSessionWriter::cancel() noexcept {
    if (m_file) {
        m_file->close();
        m_file.reset();
    }
    m_mapping = {};
    m_mappingJson.clear();
    m_lastTimestampMs = 0;
    m_recordCount = 0;
}

bool LarSessionWriter::isActive() const noexcept {
    return m_file && m_file->isOpen();
}

quint64 LarSessionWriter::recordCount() const noexcept {
    return m_recordCount;
}

bool LarSessionWriter::createSnapshot(SessionSnapshot *snapshot, QString *error) {
    if (!m_file) {
        if (error)
            *error = QStringLiteral("No recording session is active");
        return false;
    }
    if (!snapshot) {
        if (error)
            *error = QStringLiteral("Session snapshot output is null");
        return false;
    }
    if (!m_file->flush()) {
        if (error)
            *error = m_file->errorString();
        return false;
    }
    *snapshot = std::make_shared<FileSessionSnapshot>(m_file->fileName(), m_file->size(), m_file);
    return true;
}

bool LarSessionWriter::openSession(const QByteArray &mappingJson, QString *error) {
    PacketMapping mapping;
    QString mappingError;
    if (!JsonMappingRepository().loadJson(mappingJson, &mapping, &mappingError)) {
        if (error)
            *error = QStringLiteral("Invalid session mapping: %1").arg(mappingError);
        return false;
    }

    auto candidate = std::make_shared<QTemporaryFile>();
    candidate->setAutoRemove(true);
    if (!candidate->open()) {
        if (error)
            *error = candidate->errorString();
        return false;
    }

    const QByteArray canonicalMapping = mapping.json();
    const quint64 packetOffset =
        quint64(lar::session::HeaderSize) + quint64(canonicalMapping.size());
    if (packetOffset > std::numeric_limits<quint32>::max()) {
        if (error)
            *error = QStringLiteral("Session mapping exceeds the uint32 package offset");
        return false;
    }

    QByteArray header;
    header.reserve(qsizetype(packetOffset));
    header.append("LAR1", 4);
    QDataStream stream(&header, QIODevice::Append);
    configureStream(stream);
    stream << quint32(packetOffset);
    header.append(canonicalMapping);
    if (stream.status() != QDataStream::Ok || candidate->write(header) != header.size() ||
        !candidate->flush()) {

        if (error)
            *error = candidate->errorString();
        return false;
    }

    m_file = std::move(candidate);
    m_mapping = std::move(mapping);
    m_mappingJson = canonicalMapping;
    m_lastTimestampMs = 0;
    m_recordCount = 0;
    return true;
}
