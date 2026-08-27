
#include "infrastructure/session/lar_session_reader.h"
#include "application/session_limits.h"
#include "infrastructure/mapping/json_mapping_repository.h"

#include <QBuffer>
#include <QDataStream>
#include <QFile>

namespace {
void configureStream(QDataStream &stream) {
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setVersion(QDataStream::Qt_6_5);
}
} // namespace

LarSessionReader::~LarSessionReader() = default;

bool LarSessionReader::loadFile(const QString &path, QString *error) {
    close();

    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("Session file path is empty");
        return false;
    }

    auto file = std::make_unique<QFile>(path);
    if (!file->open(QIODevice::ReadOnly)) {
        if (error)
            *error = file->errorString();
        return false;
    }
    if (!parseStream(file.get(), error))
        return false;
    m_sourceFile = std::move(file);
    return true;
}

bool LarSessionReader::loadData(const QByteArray &data, QString *error) {
    close();

    QBuffer buffer;
    buffer.setData(data);
    if (!buffer.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open data buffer");
        return false;
    }
    if (!parseStream(&buffer, error))
        return false;
    m_sourceData = data;
    return true;
}

bool LarSessionReader::parseStream(QIODevice *device, QString *error) {
    if (device->size() < lar::session::HeaderSize || device->read(4) != QByteArrayLiteral("LAR1")) {
        if (error)
            *error = QStringLiteral("File magic is not LAR1");
        return false;
    }

    QDataStream stream(device);
    configureStream(stream);
    quint32 packetOffset = 0;
    stream >> packetOffset;

    if (stream.status() != QDataStream::Ok || packetOffset <= lar::session::HeaderSize ||
        packetOffset > quint64(device->size()) ||
        packetOffset - lar::session::HeaderSize > lar::session::MaximumMappingSize) {
        if (error)
            *error = QStringLiteral("Invalid UDP package-section offset");
        return false;
    }

    const QByteArray mappingJson = device->read(qint64(packetOffset) - lar::session::HeaderSize);
    PacketMapping mapping;
    JsonMappingRepository repo;
    QString mappingError;
    if (!repo.loadJson(mappingJson, &mapping, &mappingError)) {
        if (error)
            *error = QStringLiteral("Invalid embedded mapping: %1").arg(mappingError);
        return false;
    }

    if (device->pos() != packetOffset && !device->seek(packetOffset)) {
        if (error)
            *error = QStringLiteral("Cannot seek to the UDP package section");
        return false;
    }

    quint64 previousTime = 0;
    bool hasPreviousTime = false;
    qint64 recordCount = 0;
    SessionTimestamp duration;
    QVector<Checkpoint> checkpoints;
    while (!device->atEnd()) {
        const qint64 headerOffset = device->pos();
        if (device->size() - device->pos() < qint64(sizeof(quint64) + sizeof(quint32))) {
            if (error)
                *error = QStringLiteral("Truncated UDP package record header");
            return false;
        }
        quint64 relativeTimeMs = 0;
        quint32 packetSize = 0;
        stream >> relativeTimeMs >> packetSize;
        if (stream.status() != QDataStream::Ok) {
            if (error)
                *error = QStringLiteral("Invalid UDP package record header");
            return false;
        }
        if (relativeTimeMs > lar::session::MaximumDurationMilliseconds) {
            if (error) {
                *error = QStringLiteral("Session timestamp exceeds the supported 365-day limit");
            }
            return false;
        }
        if (hasPreviousTime && relativeTimeMs < previousTime) {
            if (error) {
                *error = QStringLiteral("Session package time is not monotonic");
            }
            return false;
        }
        if (packetSize == 0 || packetSize > lar::session::MaximumPacketSize ||
            qint64(packetSize) > device->size() - device->pos()) {
            if (error)
                *error = QStringLiteral("Invalid or truncated UDP package payload");
            return false;
        }
        const QByteArray packet = device->read(packetSize);
        SessionStateItem item;
        const auto timestamp = SessionTimestamp::fromStoredMilliseconds(relativeTimeMs);
        if (!timestamp) {
            if (error) {
                *error = QStringLiteral("Session timestamp exceeds the supported 365-day limit");
            }
            return false;
        }
        item.timestamp = *timestamp;
        QString packetError;
        if (!mapping.decode(packet, &item.state, &packetError)) {
            if (error) {
                *error = QStringLiteral("Stored UDP package %1 is invalid: %2")
                             .arg(recordCount)
                             .arg(packetError);
            }
            return false;
        }
        previousTime = relativeTimeMs;
        hasPreviousTime = true;
        duration = item.timestamp;
        if (recordCount % RecordsPerPage == 0)
            checkpoints.append(Checkpoint{recordCount, headerOffset, item.timestamp});
        ++recordCount;
    }

    m_mapping = std::move(mapping);
    m_mappingJson = mappingJson;
    m_checkpoints = std::move(checkpoints);
    m_page.clear();
    m_pageFirstRecord = -1;
    m_sourceSize = device->size();
    m_recordCount = recordCount;
    m_duration = duration;
    m_isValid = true;
    return true;
}

void LarSessionReader::close() noexcept {
    m_checkpoints.clear();
    m_page.clear();
    m_pageFirstRecord = -1;
    m_mapping = {};
    m_mappingJson.clear();
    m_sourceData.clear();
    if (m_sourceFile) {
        m_sourceFile->close();
        m_sourceFile.reset();
    }
    m_sourceSize = 0;
    m_recordCount = 0;
    m_duration = {};
    m_isValid = false;
}

bool LarSessionReader::ensurePage(qint64 index, QString *error) const {
    if (index < 0 || index >= m_recordCount) {
        if (error)
            *error = QStringLiteral("Session record index is out of range");
        return false;
    }

    const qint64 firstRecord = (index / RecordsPerPage) * RecordsPerPage;
    if (m_pageFirstRecord == firstRecord && index - firstRecord < m_page.size())
        return true;

    const qint64 checkpointIndex = index / RecordsPerPage;
    if (checkpointIndex < 0 || checkpointIndex >= m_checkpoints.size()) {
        if (error)
            *error = QStringLiteral("Session checkpoint index is invalid");
        return false;
    }
    const Checkpoint checkpoint = m_checkpoints.at(static_cast<qsizetype>(checkpointIndex));
    if (checkpoint.recordIndex != firstRecord) {
        if (error)
            *error = QStringLiteral("Session checkpoint table is inconsistent");
        return false;
    }

    QBuffer memoryDevice;
    QIODevice *device = nullptr;
    if (m_sourceFile) {
        device = m_sourceFile.get();
    } else {
        if (m_sourceData.size() != m_sourceSize) {
            if (error)
                *error = QStringLiteral("Session data changed after validation");
            return false;
        }
        memoryDevice.setData(m_sourceData);
        if (!memoryDevice.open(QIODevice::ReadOnly)) {
            if (error)
                *error = QStringLiteral("Cannot open session data cache");
            return false;
        }
        device = &memoryDevice;
    }
    if (!device->isOpen() || device->size() != m_sourceSize ||
        !device->seek(checkpoint.headerOffset)) {
        if (error) {
            *error = m_sourceFile ? QStringLiteral("Session file changed after validation: %1")
                                        .arg(m_sourceFile->errorString())
                                  : QStringLiteral("Session checkpoint cannot be reached");
        }
        return false;
    }

    QVector<RecordIndex> page;
    page.reserve(static_cast<qsizetype>(RecordsPerPage));
    QDataStream stream(device);
    configureStream(stream);
    const qint64 endRecord = qMin(m_recordCount, firstRecord + RecordsPerPage);
    for (qint64 current = firstRecord; current < endRecord; ++current) {
        if (device->size() - device->pos() < qint64(sizeof(quint64) + sizeof(quint32))) {
            if (error)
                *error = QStringLiteral("Session record header changed after validation");
            return false;
        }
        quint64 relativeTimeMs = 0;
        quint32 packetSize = 0;
        stream >> relativeTimeMs >> packetSize;
        const qint64 payloadOffset = device->pos();
        const auto timestamp = SessionTimestamp::fromStoredMilliseconds(relativeTimeMs);
        if (stream.status() != QDataStream::Ok || !timestamp || packetSize == 0 ||
            packetSize > lar::session::MaximumPacketSize ||
            qint64(packetSize) > device->size() - payloadOffset) {
            if (error)
                *error = QStringLiteral("Session record changed after validation");
            return false;
        }
        page.append(RecordIndex{*timestamp, payloadOffset, packetSize});
        if (!device->seek(payloadOffset + qint64(packetSize))) {
            if (error)
                *error = QStringLiteral("Session record payload cannot be skipped");
            return false;
        }
    }
    if (device->size() != m_sourceSize) {
        if (error)
            *error = QStringLiteral("Session source size changed during access");
        return false;
    }
    m_page = std::move(page);
    m_pageFirstRecord = firstRecord;
    return true;
}

const LarSessionReader::RecordIndex *LarSessionReader::locationAt(qint64 index,
                                                                  QString *error) const {
    if (!m_isValid) {
        if (error)
            *error = QStringLiteral("Session reader is not valid");
        return nullptr;
    }
    if (!ensurePage(index, error))
        return nullptr;
    const qint64 pageIndex = index - m_pageFirstRecord;
    if (pageIndex < 0 || pageIndex >= m_page.size()) {
        if (error)
            *error = QStringLiteral("Session page cache is inconsistent");
        return nullptr;
    }
    return &m_page.at(static_cast<qsizetype>(pageIndex));
}

bool LarSessionReader::findRecordAtOrBefore(SessionTimestamp position, qint64 *index,
                                            QString *error) const {
    if (!index) {
        if (error)
            *error = QStringLiteral("Session record-index output is null");
        return false;
    }
    if (!m_isValid) {
        if (error)
            *error = QStringLiteral("Session reader is not valid");
        return false;
    }
    if (m_recordCount <= 0 || m_checkpoints.isEmpty()) {
        if (error)
            *error = QStringLiteral("Session contains no records");
        return false;
    }

    // Checkpoint timestamps form a compact first-level index. Choosing the
    // last page whose first timestamp is eligible avoids the former pattern
    // of rebuilding a different 4,096-record page for every binary-search
    // probe across the complete file.
    qsizetype checkpointLow = 0;
    qsizetype checkpointHigh = m_checkpoints.size();
    while (checkpointLow < checkpointHigh) {
        const qsizetype middle = checkpointLow + (checkpointHigh - checkpointLow) / 2;
        if (m_checkpoints.at(middle).timestamp <= position)
            checkpointLow = middle + 1;
        else
            checkpointHigh = middle;
    }
    const qsizetype checkpointIndex = checkpointLow == 0 ? 0 : checkpointLow - 1;
    const Checkpoint &checkpoint = m_checkpoints.at(checkpointIndex);
    if (!ensurePage(checkpoint.recordIndex, error))
        return false;

    // The second-level search is entirely in the one bounded location page.
    // Upper-bound semantics select the last duplicate timestamp, including
    // duplicates that cross a page boundary.
    qsizetype pageLow = 0;
    qsizetype pageHigh = m_page.size();
    while (pageLow < pageHigh) {
        const qsizetype middle = pageLow + (pageHigh - pageLow) / 2;
        if (m_page.at(middle).timestamp <= position)
            pageLow = middle + 1;
        else
            pageHigh = middle;
    }
    const qsizetype pageIndex = pageLow == 0 ? 0 : pageLow - 1;
    *index = m_pageFirstRecord + static_cast<qint64>(pageIndex);
    return true;
}

bool LarSessionReader::recordAt(qint64 index, SessionStateItem *item, QString *error) const {
    if (!item) {
        if (error)
            *error = QStringLiteral("Session record output is null");
        return false;
    }
    const RecordIndex *location = locationAt(index, error);
    if (!location)
        return false;
    const RecordIndex record = *location;

    QByteArray packet;
    if (m_sourceFile) {
        if (!m_sourceFile->seek(record.payloadOffset)) {
            if (error)
                *error = m_sourceFile->errorString();
            return false;
        }
        packet = m_sourceFile->read(record.payloadSize);
    } else {
        if (record.payloadOffset < 0 ||
            record.payloadOffset > qint64(m_sourceData.size()) - qint64(record.payloadSize)) {

            if (error)
                *error = QStringLiteral("Session record range is invalid");
            return false;
        }
        packet = m_sourceData.mid(record.payloadOffset, record.payloadSize);
    }
    if (packet.size() != qint64(record.payloadSize)) {
        if (error) {
            *error = QStringLiteral("Session record payload could not be read completely");
        }
        return false;
    }

    SessionStateItem decoded;
    decoded.timestamp = record.timestamp;
    QString decodeError;
    if (!m_mapping.decode(packet, &decoded.state, &decodeError)) {
        if (error) {
            *error = QStringLiteral("Session record cannot be decoded: %1").arg(decodeError);
        }
        return false;
    }
    *item = std::move(decoded);
    return true;
}

bool LarSessionReader::timestampAt(qint64 index, SessionTimestamp *timestamp,
                                   QString *error) const {
    if (!timestamp) {
        if (error)
            *error = QStringLiteral("Session timestamp output is null");
        return false;
    }
    const RecordIndex *record = locationAt(index, error);
    if (!record)
        return false;
    *timestamp = record->timestamp;
    return true;
}
