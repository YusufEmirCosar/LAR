#include "domain/statefield.h"
#include "domain/statevalidator.h"
#include "infrastructure/mapping/json_mapping_repository.h"
#include "infrastructure/mapping/mapped_packet_decoder.h"

#include <QBitArray>
#include <QtTest>

#include <limits>

namespace {

Plane validPlane() {
    Plane plane{};
    plane.location[0] = 0.70;
    plane.location[1] = 0.51;
    plane.location[2] = 3200.0;
    plane.euler[0] = 1.20;
    plane.euler[1] = 0.03;
    plane.euler[2] = -0.01;
    plane.velocity[0] = 720.0;
    plane.velocity[1] = 15.0;
    return plane;
}

Target validTarget() {
    Target target{};
    target.iz_pos[0] = 0.701;
    target.iz_pos[1] = 0.512;
    target.iz_pos[2] = 1.0;
    target.ir_pos[0] = 0.702;
    target.ir_pos[1] = 0.513;
    target.ir_pos[2] = 1.0;
    target.iz_theta1 = -0.45;
    target.iz_theta2 = 0.65;
    target.iz_r1 = 1800.0;
    target.iz_r2 = 18000.0;
    target.ir_r = 26000.0;
    target.time = 1234.5;
    return target;
}

PacketMapping loadMapping(const QByteArray &json) {
    JsonMappingRepository repository;
    PacketMapping mapping;
    QString error;
    if (!repository.loadJson(json, &mapping, &error))
        qFatal("Cannot load test mapping: %s", qPrintable(error));
    return mapping;
}

} // namespace

class DomainTests final : public QObject {
    Q_OBJECT

  private slots:
    void structuresHaveStableSizes();
    void mappingDecodesReorderedMissingAndFloatFields();
    void mappingRejectsDuplicatesAndOverlaps();
    void validationRejectsInvalidAndNonFiniteValues();
    void decoderUsesTheConfiguredMapping();
    void dlzMappingRoundTripsAtomically();
    void stateFieldDescriptorsAreCompleteAndRoundTrip();
};

void DomainTests::structuresHaveStableSizes() {
    QCOMPARE(sizeof(Plane), size_t(72));
    QCOMPARE(sizeof(Target), size_t(96));
    QCOMPARE(StateField::Count, 24);
}

void DomainTests::mappingDecodesReorderedMissingAndFloatFields() {
    const PacketMapping mapping = loadMapping(R"([
        {"name":"location","index":1,"offset":0,"size":8},
        {"name":"location","index":0,"offset":8,"size":8},
        {"name":"iz_pos","index":0,"offset":16,"size":8},
        {"name":"iz_r1","index":0,"offset":24,"size":8}
    ])");
    QCOMPARE(mapping.bindings().size(), 4);
    QCOMPARE(mapping.minimumPacketSize(), 32);

    const Plane inputPlane = validPlane();
    const Target inputTarget = validTarget();
    Plane outputPlane{};
    Target outputTarget{};
    QBitArray available;
    QString error;
    const QByteArray packet = mapping.encode(inputPlane, inputTarget);
    QVERIFY2(mapping.decode(packet, &outputPlane, &outputTarget, &available, &error),
             qPrintable(error));
    QCOMPARE(outputPlane.location[0], inputPlane.location[0]);
    QCOMPARE(outputPlane.location[1], inputPlane.location[1]);
    QCOMPARE(outputTarget.iz_pos[0], inputTarget.iz_pos[0]);
    QCOMPARE(outputTarget.iz_r1, inputTarget.iz_r1);
    QVERIFY(available.testBit(StateField::Location0));
    QVERIFY(!available.testBit(StateField::Location2));

    QByteArray shortPacket = packet;
    shortPacket.chop(1);
    QVERIFY(!mapping.decode(shortPacket, &outputPlane, &outputTarget, &available, &error));

    QByteArray extendedPacket = packet;
    extendedPacket.append(QByteArray(12, '\0'));
    QVERIFY(mapping.decode(extendedPacket, &outputPlane, &outputTarget, &available, &error));

    const PacketMapping floatMapping =
        loadMapping(R"([{"name":"velocity","index":0,"offset":0,"size":4}])");
    QVERIFY(floatMapping.decode(floatMapping.encode(inputPlane, inputTarget), &outputPlane,
                                &outputTarget, &available, &error));
    QVERIFY(qAbs(outputPlane.velocity[0] - inputPlane.velocity[0]) < 0.001);
}

void DomainTests::mappingRejectsDuplicatesAndOverlaps() {
    JsonMappingRepository repository;
    PacketMapping mapping;
    QString error;
    QVERIFY(!repository.loadJson(R"([
        {"name":"location","index":0,"offset":0,"size":8},
        {"name":"location","index":0,"offset":8,"size":8}
    ])",
                                 &mapping, &error));
    QVERIFY(error.contains(QStringLiteral("Duplicate")));

    QVERIFY(!repository.loadJson(R"([
        {"name":"location","index":0,"offset":0,"size":8},
        {"name":"location","index":1,"offset":4,"size":8}
    ])",
                                 &mapping, &error));
    QVERIFY(error.contains(QStringLiteral("overlaps")));
}

void DomainTests::validationRejectsInvalidAndNonFiniteValues() {
    const QBitArray allFields(StateField::Count, true);
    const Plane plane = validPlane();
    Target target = validTarget();
    QString error;
    QVERIFY2(StateValidator::validate(plane, target, allFields, &error), qPrintable(error));

    target.iz_r1 = target.iz_r2;
    QVERIFY(!StateValidator::validate(plane, target, allFields, &error));
    QVERIFY(error.contains(QStringLiteral("iz_r1 < iz_r2")));

    Plane nonFinitePlane = plane;
    nonFinitePlane.velocity[2] = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!StateValidator::validate(nonFinitePlane, validTarget(), allFields, &error));
}

void DomainTests::decoderUsesTheConfiguredMapping() {
    const PacketMapping mapping = loadMapping(R"([{"name":"time","index":0,"offset":0,"size":8}])");
    MappedPacketDecoder decoder;
    decoder.setMapping(mapping);

    Plane plane{};
    Target target{};
    QBitArray available;
    QString error;
    QVERIFY2(decoder.decode(mapping.encode(validPlane(), validTarget()), &plane, &target,
                            &available, &error),
             qPrintable(error));
    QCOMPARE(target.time, validTarget().time);
    QVERIFY(available.testBit(StateField::Time));
    QVERIFY(!decoder.decode(QByteArrayLiteral("short"), &plane, &target, &available, &error));
}

void DomainTests::dlzMappingRoundTripsAtomically() {
    JsonMappingRepository repository;
    PacketMapping mapping;
    QString error;
    QVERIFY2(repository.loadJson(R"([
        {"name":"dlz_range_nm","index":0,"offset":0,"size":8},
        {"name":"dlz_aspect_deg","index":0,"offset":8,"size":8},
        {"name":"dlz_altitude_ft","index":0,"offset":16,"size":8}
    ])",
                                 &mapping, &error),
             qPrintable(error));

    DecodedState input;
    input.dlzInputs = {20.0, 90.0, 30000.0};
    input.availableFields = mapping.availableFields();
    const QByteArray packet = mapping.encode(input);
    DecodedState output;
    QVERIFY2(mapping.decode(packet, &output, &error), qPrintable(error));
    QCOMPARE(output.dlzInputs.rangeNm, 20.0);
    QCOMPARE(output.dlzInputs.aspectDegrees, 90.0);
    QCOMPARE(output.dlzInputs.altitudeFeet, 30000.0);
    QVERIFY(output.availableFields.testBit(StateField::DlzRangeNm));

    input.dlzInputs.rangeNm = 0.0;
    QVERIFY(!mapping.decode(mapping.encode(input), &output, &error));
    QVERIFY(error.contains(QStringLiteral("DLZ.range_nm")));

    input.dlzInputs = {std::numeric_limits<double>::quiet_NaN(), 90.0, 30000.0};
    QVERIFY(!mapping.decode(mapping.encode(input), &output, &error));
    QVERIFY(error.contains(QStringLiteral("DLZ.range_nm is not finite")));

    QVERIFY(!repository.loadJson(R"([
        {"name":"dlz_range_nm","index":0,"offset":0,"size":8}
    ])",
                                 &mapping, &error));
    QVERIFY(error.contains(QStringLiteral("all three fields")));
}

void DomainTests::stateFieldDescriptorsAreCompleteAndRoundTrip() {
    const auto &descriptors = StateField::all();
    QCOMPARE(descriptors.size(), std::size_t(StateField::Count));

    QSet<QString> mappingKeys;
    DecodedState state;
    for (int id = 0; id < StateField::Count; ++id) {
        const StateField::Descriptor &field = descriptors[std::size_t(id)];
        QVERIFY(field.mappingName != nullptr);
        QVERIFY(field.mappingName[0] != '\0');
        QVERIFY(field.memberName != nullptr);
        QVERIFY(field.memberName[0] != '\0');
        QVERIFY(field.presentationName != nullptr);
        QVERIFY(field.presentationName[0] != '\0');
        QVERIFY(field.getter != nullptr);
        QVERIFY(field.setter != nullptr);

        const QString key = QStringLiteral("%1:%2")
                                .arg(QString::fromLatin1(field.mappingName))
                                .arg(field.mappingIndex);
        QVERIFY2(!mappingKeys.contains(key), qPrintable(key));
        mappingKeys.insert(key);
        QCOMPARE(StateField::resolve(QString::fromLatin1(field.mappingName), field.mappingIndex),
                 id);

        const double expected = double(id) + 0.25;
        QVERIFY(StateField::setValue(&state, id, expected));
        const auto actual = StateField::tryValue(state, id);
        QVERIFY(actual.has_value());
        QCOMPARE(*actual, expected);

        using Category = StateField::ValidationCategory;
        if (field.validation == Category::LatitudeRadians ||
            field.validation == Category::LongitudeRadians) {
            QCOMPARE(field.unit, StateField::Unit::Radians);
        }
        if (field.validation == Category::DlzRangeNauticalMiles) {
            QCOMPARE(field.unit, StateField::Unit::NauticalMiles);
        }
        if (field.validation == Category::DlzAspectDegrees) {
            QCOMPARE(field.unit, StateField::Unit::Degrees);
        }
        if (field.validation == Category::DlzAltitudeFeet) {
            QCOMPARE(field.unit, StateField::Unit::Feet);
        }
    }

    QVERIFY(!StateField::tryValue(state, -1).has_value());
    QVERIFY(!StateField::tryValue(state, StateField::Count).has_value());
    QVERIFY(!StateField::setValue(&state, StateField::Count, 1.0));
    QVERIFY(std::isnan(StateField::value(state, StateField::Count)));
}

QTEST_GUILESS_MAIN(DomainTests)
#include "domain_tests.moc"
