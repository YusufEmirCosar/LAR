#include "application/metrics_service.h"
#include "application/online_capture_service.h"
#include "domain/statefield.h"
#include "infrastructure/mapping/json_mapping_repository.h"
#include "infrastructure/mapping/mapped_packet_decoder.h"
#include "infrastructure/network/qt_udp_datagram_source.h"
#include "infrastructure/runtime/threaded_application_runtime.h"
#include "testsender/scenarios.h"
#include "viewer/map/map_camera.h"
#include "viewer/viewport/lar_zone_mesh_builder.h"

#include <QHostAddress>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QtMath>
#include <QtTest>

#include <cmath>

namespace {

constexpr double Pi = 3.14159265358979323846;

class PolarUdpHarness final {
  public:
    PolarUdpHarness() : service(source, decoder, metrics) {}

    bool start(QString *error) {
        JsonMappingRepository repository;
        if (!repository.loadFile(QStringLiteral(LAR_TEST_SOURCE_DIR) +
                                     QStringLiteral("/maps/full-state.json"),
                                 &mapping, error)) {
            return false;
        }
        service.setMapping(mapping);
        return service.start(0, error);
    }

    bool send(const Plane &plane, const Target &target, QString *error) {
        const QByteArray packet = mapping.encode(plane, target);
        const qint64 written =
            sender.writeDatagram(packet, QHostAddress::LocalHost, service.port());
        if (written != packet.size()) {
            if (error) {
                *error = sender.errorString();
            }
            return false;
        }
        return true;
    }

    QtUdpDatagramSource source;
    MappedPacketDecoder decoder;
    MetricsService metrics;
    OnlineCaptureService service;
    PacketMapping mapping;
    QUdpSocket sender;
};

void scenarioState(const QString &name, int packetIndex, double elapsedSeconds, Plane *plane,
                   Target *target) {
    TestSenderScenarios::initialize(name, plane, target);
    TestSenderScenarios::update(name, packetIndex, elapsedSeconds, plane, target);
}

Target publishedTarget(const QList<QVariant> &arguments) {
    return qvariant_cast<DecodedState>(arguments.at(0)).target;
}

Plane publishedPlane(const QList<QVariant> &arguments) {
    return qvariant_cast<DecodedState>(arguments.at(0)).plane;
}

void verifyPolarMeshes(const Target &target) {
    for (const lar::map::MapPresentation presentation :
         {lar::map::MapPresentation::Mercator, lar::map::MapPresentation::Sphere}) {
        lar::map::MapCamera camera;
        camera.setPresentation(presentation);
        const LarZoneMesh mesh = LarZoneMeshBuilder().build(
            target, QBitArray(StateField::Count, true), camera, 1200, 800);

        QVERIFY(!mesh.empty());
        QVERIFY(!mesh.inputRejected);
        QVERIFY(mesh.inRangeFill.indexCount > 0U);
        QVERIFY(mesh.inZoneFill.indexCount > 0U);
        QVERIFY(mesh.vertices.size() / 3U <= LarZoneMeshBuilder::MaximumVertexCount);
        QVERIFY(mesh.indices.size() <= LarZoneMeshBuilder::MaximumIndexCount);
    }
}

} // namespace

class PolarUdpTests final : public QObject {
    Q_OBJECT

  private slots:
    void movingPolarLarPacketsReachBothProjectionMeshes();

    void poleCrossingRouteSurvivesUdpParsing();

    void coordinatedTurnHighRateKeepsLarCentersCoherent();
};

void PolarUdpTests::movingPolarLarPacketsReachBothProjectionMeshes() {
    PolarUdpHarness harness;
    QString error;
    if (!harness.start(&error)) {
        QSKIP(qPrintable(QStringLiteral("Loopback UDP is unavailable: %1").arg(error)));
    }
    QSignalSpy stateSpy(&harness.service, &OnlineCaptureService::stateReceived);
    QSignalSpy errorSpy(&harness.service, &OnlineCaptureService::captureError);

    double previousIrRadius = -1.0;
    double previousIzRadius = -1.0;
    for (const int packetIndex : {0, 20, 40, 60, 80}) {
        Plane expectedPlane{};
        Target expectedTarget{};
        scenarioState(QStringLiteral("polar-lar-stress"), packetIndex,
                      static_cast<double>(packetIndex) * 0.6, &expectedPlane, &expectedTarget);
        QVERIFY2(harness.send(expectedPlane, expectedTarget, &error), qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(!stateSpy.isEmpty(), 1000);

        const Target actualTarget = publishedTarget(stateSpy.takeFirst());
        QCOMPARE(actualTarget.ir_pos[0], expectedTarget.ir_pos[0]);
        QCOMPARE(actualTarget.ir_pos[1], expectedTarget.ir_pos[1]);
        QCOMPARE(actualTarget.ir_r, expectedTarget.ir_r);
        QCOMPARE(actualTarget.iz_pos[0], expectedTarget.iz_pos[0]);
        QCOMPARE(actualTarget.iz_pos[1], expectedTarget.iz_pos[1]);
        QCOMPARE(actualTarget.iz_r2, expectedTarget.iz_r2);
        QVERIFY(actualTarget.ir_r > 900000.0);
        QVERIFY(actualTarget.iz_r2 > 650000.0);
        QVERIFY(std::abs(actualTarget.ir_pos[0]) > qDegreesToRadians(70.0));
        QVERIFY(std::abs(actualTarget.iz_pos[0]) > qDegreesToRadians(70.0));
        if (previousIrRadius >= 0.0) {
            QVERIFY(actualTarget.ir_r != previousIrRadius);
            QVERIFY(actualTarget.iz_r2 != previousIzRadius);
        }
        previousIrRadius = actualTarget.ir_r;
        previousIzRadius = actualTarget.iz_r2;
        verifyPolarMeshes(actualTarget);
    }

    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(harness.metrics.totalProcessedPackets(), quint64(5));
}

void PolarUdpTests::poleCrossingRouteSurvivesUdpParsing() {
    PolarUdpHarness harness;
    QString error;
    if (!harness.start(&error)) {
        QSKIP(qPrintable(QStringLiteral("Loopback UDP is unavailable: %1").arg(error)));
    }
    QSignalSpy stateSpy(&harness.service, &OnlineCaptureService::stateReceived);
    QSignalSpy errorSpy(&harness.service, &OnlineCaptureService::captureError);

    double northBeforeLongitude = 0.0;
    double northAfterLongitude = 0.0;
    double southBeforeLongitude = 0.0;
    double southAfterLongitude = 0.0;
    for (const int packetIndex : {3, 4, 5, 39, 40, 41}) {
        Plane expectedPlane{};
        Target expectedTarget{};
        scenarioState(QStringLiteral("pole-crossing-route"), packetIndex,
                      static_cast<double>(packetIndex) * 0.05, &expectedPlane, &expectedTarget);
        QVERIFY2(harness.send(expectedPlane, expectedTarget, &error), qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(!stateSpy.isEmpty(), 1000);

        const Plane actualPlane = publishedPlane(stateSpy.takeFirst());
        QCOMPARE(actualPlane.location[0], expectedPlane.location[0]);
        QCOMPARE(actualPlane.location[1], expectedPlane.location[1]);
        QVERIFY(std::isfinite(actualPlane.location[0]));
        QVERIFY(std::isfinite(actualPlane.location[1]));

        if (packetIndex == 3)
            northBeforeLongitude = actualPlane.location[1];
        if (packetIndex == 4) {
            QVERIFY(std::abs(actualPlane.location[0] - Pi * 0.5) < 1.0e-12);
        }
        if (packetIndex == 5)
            northAfterLongitude = actualPlane.location[1];
        if (packetIndex == 39)
            southBeforeLongitude = actualPlane.location[1];
        if (packetIndex == 40) {
            QVERIFY(std::abs(actualPlane.location[0] + Pi * 0.5) < 1.0e-12);
        }
        if (packetIndex == 41)
            southAfterLongitude = actualPlane.location[1];
    }

    QVERIFY(std::abs(northAfterLongitude - northBeforeLongitude) > 3.0);
    QVERIFY(std::abs(southAfterLongitude - southBeforeLongitude) > 3.0);
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(harness.metrics.totalProcessedPackets(), quint64(6));
}

void PolarUdpTests::coordinatedTurnHighRateKeepsLarCentersCoherent() {
    const QString mappingPath =
        QStringLiteral(LAR_TEST_SOURCE_DIR) + QStringLiteral("/maps/full-state.json");
    PacketMapping mapping;
    QString mappingError;
    JsonMappingRepository repository;
    QVERIFY2(repository.loadFile(mappingPath, &mapping, &mappingError), qPrintable(mappingError));

    QUdpSocket portProbe;
    if (!portProbe.bind(QHostAddress::LocalHost, 0)) {
        QSKIP(qPrintable(
            QStringLiteral("Loopback UDP is unavailable: %1").arg(portProbe.errorString())));
    }
    const quint16 port = portProbe.localPort();
    QVERIFY(port != 0);
    portProbe.close();

    ThreadedApplicationRuntime runtime;
    QSignalSpy mappingSpy(&runtime, &IApplicationRuntime::mappingLoadFinished);
    QSignalSpy startSpy(&runtime, &IApplicationRuntime::onlineStartFinished);
    QSignalSpy stateSpy(&runtime, &IApplicationRuntime::stateReady);
    QSignalSpy errorSpy(&runtime, &IApplicationRuntime::runtimeError);

    QVERIFY(runtime.loadMapping(mappingPath));
    QTRY_COMPARE_WITH_TIMEOUT(mappingSpy.count(), 1, 2000);
    QVERIFY(qvariant_cast<MappingLoadResult>(mappingSpy.at(0).at(0)).loaded);
    QVERIFY(runtime.startOnline(port));
    QTRY_COMPARE_WITH_TIMEOUT(startSpy.count(), 1, 2000);
    const auto startResult = qvariant_cast<OnlineStartResult>(startSpy.at(0).at(0));
    QVERIFY2(startResult.started, qPrintable(startResult.error));

    QUdpSocket sender;
    constexpr int PacketCount = 250;
    constexpr double IntervalSeconds = 0.002;
    for (int packetIndex = 0; packetIndex < PacketCount; ++packetIndex) {
        Plane plane{};
        Target target{};
        scenarioState(QStringLiteral("coordinated-turn"), packetIndex,
                      static_cast<double>(packetIndex) * IntervalSeconds, &plane, &target);
        const QByteArray datagram = mapping.encode(plane, target);
        QCOMPARE(sender.writeDatagram(datagram, QHostAddress::LocalHost, port),
                 static_cast<qint64>(datagram.size()));
        QTest::qWait(2);
    }

    QTRY_VERIFY_WITH_TIMEOUT(!stateSpy.isEmpty(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(
        qvariant_cast<StateEvent>(stateSpy.constLast().at(0)).state.target.time >=
            static_cast<double>(PacketCount - 1) * IntervalSeconds,
        2000);

    QVERIFY(stateSpy.count() >= 10);
    const QBitArray allFields = mapping.availableFields();
    for (const QList<QVariant> &arguments : stateSpy) {
        const DecodedState actualState = qvariant_cast<StateEvent>(arguments.at(0)).state;
        const Plane actualPlane = actualState.plane;
        const Target actualTarget = actualState.target;
        const QBitArray actualFields = actualState.availableFields;
        QCOMPARE(actualFields, allFields);

        const int packetIndex = static_cast<int>(std::llround(actualTarget.time / IntervalSeconds));
        QVERIFY(packetIndex >= 0);
        QVERIFY(packetIndex < PacketCount);
        Plane expectedPlane{};
        Target expectedTarget{};
        scenarioState(QStringLiteral("coordinated-turn"), packetIndex,
                      static_cast<double>(packetIndex) * IntervalSeconds, &expectedPlane,
                      &expectedTarget);
        for (int field = 0; field <= StateField::Time; ++field) {
            QCOMPARE(StateField::value(actualPlane, actualTarget, field),
                     StateField::value(expectedPlane, expectedTarget, field));
        }
        QVERIFY(std::abs(actualTarget.ir_pos[0] - actualTarget.iz_pos[0]) < 0.001);
        QVERIFY(std::abs(actualTarget.ir_pos[1] - actualTarget.iz_pos[1]) < 0.001);
    }

    QCOMPARE(errorSpy.count(), 0);
    runtime.stopOnline();
    runtime.shutdown();
}

QTEST_GUILESS_MAIN(PolarUdpTests)

#include "polar_udp_tests.moc"
