
#include "infrastructure/mapping/json_mapping_repository.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QStringList>

#include <cmath>

namespace {

constexpr int MaximumPacketSize = 16 * 1024 * 1024;
constexpr qint64 MaximumMappingSize = 16 * 1024 * 1024;

bool jsonInteger(const QJsonObject &object, const QString &key, int *value) {
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isDouble() || std::floor(jsonValue.toDouble()) != jsonValue.toDouble())
        return false;
    *value = jsonValue.toInt(-1);
    return true;
}

} // namespace

bool JsonMappingRepository::loadFile(const QString &path, PacketMapping *mapping, QString *error) {
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("Mapping file path is empty");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (file.size() <= 0 || file.size() > MaximumMappingSize) {
        if (error)
            *error = QStringLiteral("Mapping file is empty or exceeds 16 MiB");
        return false;
    }
    return loadJson(file.readAll(), mapping, error);
}

bool JsonMappingRepository::loadJson(const QByteArray &json, PacketMapping *mapping,
                                     QString *error) {
    if (!mapping) {
        if (error)
            *error = QStringLiteral("Target mapping pointer is null");
        return false;
    }
    if (json.isEmpty() || json.size() > MaximumMappingSize) {
        if (error)
            *error = QStringLiteral("Mapping JSON is empty or exceeds 16 MiB");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (!document.isArray()) {
        if (error) {
            *error = (parseError.error == QJsonParseError::NoError)
                         ? QStringLiteral("Mapping root must be a JSON array")
                         : parseError.errorString();
        }
        return false;
    }
    const QJsonArray entries = document.array();
    if (entries.isEmpty()) {
        if (error)
            *error = QStringLiteral("Mapping must contain at least one field");
        return false;
    }

    QVector<FieldBinding> bindings;
    QSet<int> seenFields;

    for (int entryIndex = 0; entryIndex < entries.size(); ++entryIndex) {
        if (!entries.at(entryIndex).isObject()) {
            if (error)
                *error = QStringLiteral("Mapping entry %1 must be an object").arg(entryIndex);
            return false;
        }
        const QJsonObject entry = entries.at(entryIndex).toObject();
        const QString name = entry.value(QStringLiteral("name")).toString();
        int index = -1;
        int offset = -1;
        int size = -1;
        if (name.isEmpty() || !jsonInteger(entry, QStringLiteral("index"), &index) ||
            !jsonInteger(entry, QStringLiteral("offset"), &offset) ||
            !jsonInteger(entry, QStringLiteral("size"), &size)) {

            if (error)
                *error = QStringLiteral("Mapping entry %1 requires name, index, offset, and size")
                             .arg(entryIndex);
            return false;
        }
        const int fieldId = StateField::resolve(name, index);
        if (fieldId < 0) {
            if (error)
                *error =
                    QStringLiteral("Unknown field or invalid index at mapping entry %1: %2[%3]")
                        .arg(entryIndex)
                        .arg(name)
                        .arg(index);
            return false;
        }
        if (seenFields.contains(fieldId)) {
            if (error)
                *error = QStringLiteral("Duplicate mapping for %1")
                             .arg(StateField::displayName(fieldId));
            return false;
        }
        if (offset < 0 || (size != 4 && size != 8) || offset > MaximumPacketSize - size) {
            if (error)
                *error = QStringLiteral("Entry %1 must use a non-negative offset and size 4 or 8")
                             .arg(entryIndex);
            return false;
        }
        for (const auto &b : bindings) {
            if (offset < b.offset + b.size && offset + size > b.offset) {
                if (error)
                    *error = QStringLiteral("Byte range for %1 overlaps another mapped field")
                                 .arg(StateField::displayName(fieldId));
                return false;
            }
        }
        seenFields.insert(fieldId);
        bindings.append(FieldBinding{fieldId, offset, size});
    }

    const bool hasDlzRange = seenFields.contains(StateField::DlzRangeNm);
    const bool hasDlzAspect = seenFields.contains(StateField::DlzAspectDegrees);
    const bool hasDlzAltitude = seenFields.contains(StateField::DlzAltitudeFeet);
    const int dlzFieldCount = int(hasDlzRange) + int(hasDlzAspect) + int(hasDlzAltitude);
    if (dlzFieldCount != 0 && dlzFieldCount != 3) {
        QStringList missing;
        if (!hasDlzRange)
            missing.append(QStringLiteral("dlz_range_nm"));
        if (!hasDlzAspect)
            missing.append(QStringLiteral("dlz_aspect_deg"));
        if (!hasDlzAltitude)
            missing.append(QStringLiteral("dlz_altitude_ft"));
        if (error) {
            *error = QStringLiteral("DLZ mapping must contain all three fields; missing: %1")
                         .arg(missing.join(QStringLiteral(", ")));
        }
        return false;
    }

    PacketMapping parsed(bindings, document.toJson(QJsonDocument::Compact));
    if (!parsed.isValid()) {
        if (error)
            *error = QStringLiteral("Mapping violates packet binding invariants");
        return false;
    }
    *mapping = std::move(parsed);
    return true;
}
