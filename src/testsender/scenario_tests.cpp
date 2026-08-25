
#include "domain/statevalidator.h"
#include "infrastructure/mapping/json_mapping_repository.h"
#include "testsender/scenarios.h"

#include <QBitArray>
#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double EarthRadiusMeters = 6371000.0;
constexpr double MaximumLarCenterSeparationMeters = 1000.0;

bool statesEqual(const Plane &leftPlane, const Target &leftTarget, const Plane &rightPlane,
                 const Target &rightTarget) {
    for (int field = 0; field <= StateField::Time; ++field) {
        if (StateField::value(leftPlane, leftTarget, field) !=
            StateField::value(rightPlane, rightTarget, field)) {
            return false;
        }
    }
    return true;
}

bool dlzValuesEqual(const dlz::TelemetryInputs &left, const dlz::TelemetryInputs &right) {
    return left.rangeNm == right.rangeNm && left.aspectDegrees == right.aspectDegrees &&
           left.altitudeFeet == right.altitudeFeet;
}

double larCenterSeparationMeters(const Target &target) {
    const double latitudeDelta = target.ir_pos[0] - target.iz_pos[0];
    const double longitudeDelta = std::remainder(target.ir_pos[1] - target.iz_pos[1], 2.0 * Pi);
    const double sinHalfLatitude = std::sin(latitudeDelta * 0.5);
    const double sinHalfLongitude = std::sin(longitudeDelta * 0.5);
    const double haversine =
        std::clamp(sinHalfLatitude * sinHalfLatitude + std::cos(target.ir_pos[0]) *
                                                           std::cos(target.iz_pos[0]) *
                                                           sinHalfLongitude * sinHalfLongitude,
                   0.0, 1.0);
    return 2.0 * EarthRadiusMeters * std::asin(std::sqrt(haversine));
}

bool checkState(const QString &scenario, int packetIndex, double elapsedSeconds,
                const PacketMapping &floatMapping, const PacketMapping &dlzMapping,
                const PacketMapping &combinedMapping) {
    Plane firstPlane{};
    Target firstTarget{};
    dlz::TelemetryInputs firstDlz;
    TestSenderScenarios::initialize(scenario, &firstPlane, &firstTarget, &firstDlz);
    TestSenderScenarios::update(scenario, packetIndex, elapsedSeconds, &firstPlane, &firstTarget,
                                &firstDlz);

    QBitArray allFields(StateField::Count, true);
    QString validationError;
    if (!StateValidator::validate(firstPlane, firstTarget, allFields, &validationError)) {
        QTextStream(stderr) << scenario << " packet " << packetIndex
                            << " is invalid: " << validationError << '\n';
        return false;
    }
    DecodedState firstDecoded;
    firstDecoded.dlzInputs = firstDlz;
    firstDecoded.availableFields = QBitArray(StateField::Count);
    firstDecoded.availableFields.setBit(StateField::DlzRangeNm);
    firstDecoded.availableFields.setBit(StateField::DlzAspectDegrees);
    firstDecoded.availableFields.setBit(StateField::DlzAltitudeFeet);
    if (!StateValidator::validate(firstDecoded, &validationError)) {
        QTextStream(stderr) << scenario << " packet " << packetIndex
                            << " has invalid DLZ telemetry: " << validationError << '\n';
        return false;
    }
    const double centerSeparation = larCenterSeparationMeters(firstTarget);
    if (!std::isfinite(centerSeparation) || centerSeparation > MaximumLarCenterSeparationMeters) {
        QTextStream(stderr) << scenario << " packet " << packetIndex
                            << " separates IR and IZ centers by " << centerSeparation
                            << " meters\n";
        return false;
    }

    Plane mappedPlane{};
    Target mappedTarget{};
    QBitArray mappedFields;
    if (!floatMapping.decode(floatMapping.encode(firstPlane, firstTarget), &mappedPlane,
                             &mappedTarget, &mappedFields, &validationError)) {
        QTextStream(stderr) << scenario << " packet " << packetIndex
                            << " fails float mapping round-trip: " << validationError << '\n';
        return false;
    }

    Plane secondPlane{};
    Target secondTarget{};
    dlz::TelemetryInputs secondDlz;
    TestSenderScenarios::initialize(scenario, &secondPlane, &secondTarget, &secondDlz);
    TestSenderScenarios::update(scenario, packetIndex, elapsedSeconds, &secondPlane, &secondTarget,
                                &secondDlz);
    if (!statesEqual(firstPlane, firstTarget, secondPlane, secondTarget) ||
        !dlzValuesEqual(firstDlz, secondDlz)) {
        QTextStream(stderr) << scenario << " packet " << packetIndex << " is not deterministic\n";
        return false;
    }

    DecodedState aggregate;
    aggregate.plane = firstPlane;
    aggregate.target = firstTarget;
    aggregate.dlzInputs = firstDlz;
    for (const PacketMapping *mapping : {&dlzMapping, &combinedMapping}) {
        aggregate.availableFields = mapping->availableFields();
        const QByteArray encoded = mapping->encode(aggregate);
        DecodedState decoded;
        if (!mapping->decode(encoded, &decoded, &validationError)) {
            QTextStream(stderr) << scenario << " packet " << packetIndex
                                << " fails DLZ mapping round-trip: " << validationError << '\n';
            return false;
        }
        if (decoded.dlzInputs.rangeNm != firstDlz.rangeNm ||
            decoded.dlzInputs.aspectDegrees != firstDlz.aspectDegrees ||
            decoded.dlzInputs.altitudeFeet != firstDlz.altitudeFeet) {
            QTextStream(stderr) << scenario << " packet " << packetIndex
                                << " changes DLZ telemetry during round-trip\n";
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    const QByteArray floatMapJson = R"json([
        {"name":"location","index":0,"offset":0,"size":4},
        {"name":"location","index":1,"offset":4,"size":4},
        {"name":"location","index":2,"offset":8,"size":4},
        {"name":"euler","index":0,"offset":12,"size":4},
        {"name":"euler","index":1,"offset":16,"size":4},
        {"name":"euler","index":2,"offset":20,"size":4},
        {"name":"velocity","index":0,"offset":24,"size":4},
        {"name":"velocity","index":1,"offset":28,"size":4},
        {"name":"velocity","index":2,"offset":32,"size":4},
        {"name":"iz_pos","index":0,"offset":36,"size":4},
        {"name":"iz_pos","index":1,"offset":40,"size":4},
        {"name":"iz_pos","index":2,"offset":44,"size":4},
        {"name":"ir_pos","index":0,"offset":48,"size":4},
        {"name":"ir_pos","index":1,"offset":52,"size":4},
        {"name":"ir_pos","index":2,"offset":56,"size":4},
        {"name":"iz_theta1","index":0,"offset":60,"size":4},
        {"name":"iz_theta2","index":0,"offset":64,"size":4},
        {"name":"iz_r1","index":0,"offset":68,"size":4},
        {"name":"iz_r2","index":0,"offset":72,"size":4},
        {"name":"ir_r","index":0,"offset":76,"size":4},
        {"name":"time","index":0,"offset":80,"size":4}
    ])json";
    PacketMapping floatMapping;
    JsonMappingRepository mappingRepository;
    QString mappingError;
    if (!mappingRepository.loadJson(floatMapJson, &floatMapping, &mappingError)) {
        QTextStream(stderr) << "Unable to load test float mapping: " << mappingError << '\n';
        return 1;
    }

    const QByteArray dlzMapJson = R"json([
        {"name":"dlz_range_nm","index":0,"offset":0,"size":8},
        {"name":"dlz_aspect_deg","index":0,"offset":8,"size":8},
        {"name":"dlz_altitude_ft","index":0,"offset":16,"size":8}
    ])json";
    PacketMapping dlzMapping;
    if (!mappingRepository.loadJson(dlzMapJson, &dlzMapping, &mappingError)) {
        QTextStream(stderr) << "Unable to load DLZ mapping: " << mappingError << '\n';
        return 1;
    }
    const QByteArray combinedMapJson = R"json([
        {"name":"location","index":0,"offset":0,"size":8},
        {"name":"location","index":1,"offset":8,"size":8},
        {"name":"iz_pos","index":0,"offset":16,"size":8},
        {"name":"iz_r1","index":0,"offset":24,"size":8},
        {"name":"euler","index":0,"offset":32,"size":8},
        {"name":"ir_pos","index":1,"offset":40,"size":8},
        {"name":"iz_theta2","index":0,"offset":48,"size":8},
        {"name":"velocity","index":2,"offset":56,"size":8},
        {"name":"iz_pos","index":1,"offset":64,"size":8},
        {"name":"location","index":2,"offset":72,"size":8},
        {"name":"ir_r","index":0,"offset":80,"size":8},
        {"name":"euler","index":2,"offset":88,"size":8},
        {"name":"ir_pos","index":0,"offset":96,"size":8},
        {"name":"velocity","index":0,"offset":104,"size":8},
        {"name":"iz_r2","index":0,"offset":112,"size":8},
        {"name":"iz_pos","index":2,"offset":120,"size":8},
        {"name":"euler","index":1,"offset":128,"size":8},
        {"name":"time","index":0,"offset":136,"size":8},
        {"name":"velocity","index":1,"offset":144,"size":8},
        {"name":"iz_theta1","index":0,"offset":152,"size":8},
        {"name":"ir_pos","index":2,"offset":160,"size":8},
        {"name":"dlz_range_nm","index":0,"offset":168,"size":8},
        {"name":"dlz_aspect_deg","index":0,"offset":176,"size":8},
        {"name":"dlz_altitude_ft","index":0,"offset":184,"size":8}
    ])json";
    PacketMapping combinedMapping;
    if (!mappingRepository.loadJson(combinedMapJson, &combinedMapping, &mappingError)) {
        QTextStream(stderr) << "Unable to load combined DLZ mapping: " << mappingError << '\n';
        return 1;
    }

    const QList<int> packetIndexes = {0, 1, 10, 100, 1000, 1000000, 2000000000};
    for (const QString &scenario : TestSenderScenarios::names()) {
        if (TestSenderScenarios::description(scenario).isEmpty()) {
            QTextStream(stderr) << scenario << " has no description\n";
            return 1;
        }
        for (int packetIndex : packetIndexes) {
            const double elapsedSeconds = double(packetIndex) * 2147483.0;
            if (!checkState(scenario, packetIndex, elapsedSeconds, floatMapping, dlzMapping,
                            combinedMapping))
                return 1;
        }
    }

    Plane defaultPlane{};
    Target defaultTarget{};
    TestSenderScenarios::initialize(QStringLiteral("default"), &defaultPlane, &defaultTarget);
    TestSenderScenarios::update(QStringLiteral("default"), 0, 0.0, &defaultPlane, &defaultTarget);
    if (defaultTarget.ir_r != 26000.0) {
        QTextStream(stderr) << "Default scenario must retain Target.ir_r = 26000\n";
        return 1;
    }
    if (std::abs(defaultTarget.ir_pos[0] - 0.71575) > 1.0e-12 ||
        std::abs(defaultTarget.ir_pos[1] - 0.50530) > 1.0e-12 ||
        std::abs(defaultTarget.ir_pos[0] - defaultTarget.iz_pos[0]) > 0.001 ||
        std::abs(defaultTarget.ir_pos[1] - defaultTarget.iz_pos[1]) > 0.001) {

        QTextStream(stderr) << "Default IR and IZ centers must remain locally coherent\n";
        return 1;
    }
    if (TestSenderScenarios::defaultIntervalMs(QStringLiteral("hundred-hz")) != 10 ||
        TestSenderScenarios::defaultIntervalMs(QStringLiteral("default")) != 100) {

        QTextStream(stderr) << "Scenario default intervals are incorrect\n";
        return 1;
    }

    Plane polarPlane{};
    Target polarTarget{};
    TestSenderScenarios::initialize(QStringLiteral("polar-lar-stress"), &polarPlane, &polarTarget);
    const double firstIrRadius = polarTarget.ir_r;
    const double firstIzRadius = polarTarget.iz_r2;
    TestSenderScenarios::update(QStringLiteral("polar-lar-stress"), 20, 12.0, &polarPlane,
                                &polarTarget);
    if (polarTarget.ir_r == firstIrRadius || polarTarget.iz_r2 == firstIzRadius ||
        std::abs(polarTarget.ir_pos[0]) < 70.0 * 3.14159265358979323846 / 180.0 ||
        std::abs(polarTarget.iz_pos[0]) < 70.0 * 3.14159265358979323846 / 180.0) {

        QTextStream(stderr) << "Polar LAR stress scenario must move near-pole zones "
                               "and change both radii\n";
        return 1;
    }

    Plane routePlane{};
    Target routeTarget{};
    TestSenderScenarios::initialize(QStringLiteral("pole-crossing-route"), &routePlane,
                                    &routeTarget);
    TestSenderScenarios::update(QStringLiteral("pole-crossing-route"), 4, 0.2, &routePlane,
                                &routeTarget);
    if (routePlane.location[0] < 89.999 * 3.14159265358979323846 / 180.0) {
        QTextStream(stderr) << "Pole-crossing route must reach the north pole\n";
        return 1;
    }
    TestSenderScenarios::update(QStringLiteral("pole-crossing-route"), 40, 2.0, &routePlane,
                                &routeTarget);
    if (routePlane.location[0] > -89.999 * 3.14159265358979323846 / 180.0) {
        QTextStream(stderr) << "Pole-crossing route must reach the south pole\n";
        return 1;
    }

    return 0;
}
