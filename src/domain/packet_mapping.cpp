
#include "domain/packet_mapping.h"
#include "domain/statevalidator.h"

#include <QtEndian>
#include <cmath>
#include <cstring>
#include <limits>
namespace {

constexpr int MaximumPacketSize = 16 * 1024 * 1024;

double readNumber(const char *data, int size) {
    if (size == 4) {
        quint32 bits = 0;
        std::memcpy(&bits, data, sizeof(bits));
        bits = qFromLittleEndian(bits);
        float value = 0.0F;
        std::memcpy(&value, &bits, sizeof(value));
        return double(value);
    }
    quint64 bits = 0;
    std::memcpy(&bits, data, sizeof(bits));
    bits = qFromLittleEndian(bits);
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void writeNumber(char *data, int size, double value) {
    if (size == 4) {
        const float converted = float(value);
        quint32 bits = 0;
        std::memcpy(&bits, &converted, sizeof(bits));
        bits = qToLittleEndian(bits);
        std::memcpy(data, &bits, sizeof(bits));
        return;
    }
    quint64 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    bits = qToLittleEndian(bits);
    std::memcpy(data, &bits, sizeof(bits));
}

} // namespace

PacketMapping::PacketMapping(QVector<FieldBinding> bindings, QByteArray json)
    : m_availableFields(StateField::Count) {
    QBitArray seenFields(StateField::Count);
    for (qsizetype i = 0; i < bindings.size(); ++i) {
        const auto &binding = bindings.at(i);
        if (binding.fieldId < 0 || binding.fieldId >= StateField::Count || binding.offset < 0 ||
            (binding.size != 4 && binding.size != 8) ||
            binding.offset > std::numeric_limits<int>::max() - binding.size ||
            binding.offset > MaximumPacketSize - binding.size ||
            seenFields.testBit(binding.fieldId)) {
            return;
        }
        for (qsizetype j = 0; j < i; ++j) {
            const auto &other = bindings.at(j);
            if (binding.offset < other.offset + other.size &&
                binding.offset + binding.size > other.offset) {
                return;
            }
        }
        seenFields.setBit(binding.fieldId);
        m_minimumPacketSize = qMax(m_minimumPacketSize, binding.offset + binding.size);
    }
    if (bindings.isEmpty() || json.isEmpty())
        return;
    const int dlzFieldCount = int(seenFields.testBit(StateField::DlzRangeNm)) +
                              int(seenFields.testBit(StateField::DlzAspectDegrees)) +
                              int(seenFields.testBit(StateField::DlzAltitudeFeet));
    if (dlzFieldCount != 0 && dlzFieldCount != 3)
        return;
    m_bindings = std::move(bindings);
    m_availableFields = std::move(seenFields);
    m_json = std::move(json);
    m_valid = true;
}

bool PacketMapping::decode(const QByteArray &packet, Plane *plane, Target *target,
                           QBitArray *availableFields, QString *error) const {
    if (!isValid() || !plane || !target || !availableFields) {
        if (error)
            *error = QStringLiteral("A valid mapping and output objects are required");
        return false;
    }
    DecodedState state;
    if (!decode(packet, &state, error))
        return false;
    *plane = state.plane;
    *target = state.target;
    *availableFields = state.availableFields;
    return true;
}

bool PacketMapping::decode(const QByteArray &packet, DecodedState *state, QString *error) const {
    if (!isValid() || state == nullptr) {
        if (error)
            *error = QStringLiteral("A valid mapping and output state are required");
        return false;
    }
    if (packet.size() < m_minimumPacketSize) {
        if (error) {
            *error = QStringLiteral("Mapped packet requires at least %1 bytes; received %2")
                         .arg(m_minimumPacketSize)
                         .arg(packet.size());
        }
        return false;
    }
    DecodedState decoded;
    decoded.availableFields = m_availableFields;
    for (const auto &binding : m_bindings) {
        StateField::setValue(&decoded, binding.fieldId,
                             readNumber(packet.constData() + binding.offset, binding.size));
    }
    QString validationError;
    if (!StateValidator::validate(decoded, &validationError)) {
        if (error)
            *error = validationError;
        return false;
    }
    *state = std::move(decoded);
    return true;
}

QByteArray PacketMapping::encode(const Plane &plane, const Target &target) const {
    if (!isValid())
        return {};
    QByteArray packet(m_minimumPacketSize, '\0');
    for (const auto &binding : m_bindings) {
        writeNumber(packet.data() + binding.offset, binding.size,
                    StateField::value(plane, target, binding.fieldId));
    }
    return packet;
}

QByteArray PacketMapping::encode(const DecodedState &state) const {
    if (!isValid())
        return {};
    QByteArray packet(m_minimumPacketSize, '\0');
    for (const auto &binding : m_bindings) {
        writeNumber(packet.data() + binding.offset, binding.size,
                    StateField::value(state, binding.fieldId));
    }
    return packet;
}
