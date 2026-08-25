#include "tests/support/architecture_test_support.h"

class NetworkAdapterTests final : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void qtDatagramSourceSatisfiesTransportContract();
};

void NetworkAdapterTests::initTestCase() {
    qRegisterMetaType<SessionSnapshot>("SessionSnapshot");
    qRegisterMetaType<QVector<CapturedPacket>>("QVector<CapturedPacket>");
    qRegisterMetaType<RuntimeStateSource>("RuntimeStateSource");
    qRegisterMetaType<IpAccessPolicy>("IpAccessPolicy");
}

void NetworkAdapterTests::qtDatagramSourceSatisfiesTransportContract() {
    QtUdpDatagramSource source;
    MappedPacketDecoder decoder;
    MetricsService metrics;
    OnlineCaptureService capture(source, decoder, metrics);
    const PacketMapping mapping = timeOnlyMapping();
    capture.setMapping(mapping);
    QSignalSpy rawSpy(&capture, &OnlineCaptureService::rawPacketReceived);
    QSignalSpy stateSpy(&capture, &OnlineCaptureService::stateReceived);
    QSignalSpy errorSpy(&capture, &OnlineCaptureService::captureError);

    QString error;
    if (!capture.start(0, &error)) {
        QSKIP(
            qPrintable(QStringLiteral("Loopback UDP binding is unavailable in this environment: %1")
                           .arg(error)));
    }
    QVERIFY(source.isListening());
    QVERIFY(source.localPort() != 0);

    Plane plane{};
    Target target{};
    target.time = 17.0;
    const QByteArray validPacket = mapping.encode(plane, target);
    QUdpSocket sender;
    QCOMPARE(sender.writeDatagram(validPacket, QHostAddress::LocalHost, source.localPort()),
             qint64(validPacket.size()));
    QCOMPARE(
        sender.writeDatagram(QByteArrayLiteral("bad"), QHostAddress::LocalHost, source.localPort()),
        qint64(3));

    QTRY_COMPARE_WITH_TIMEOUT(rawSpy.size(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(stateSpy.size(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(errorSpy.size(), 1, 2000);
    QCOMPARE(qvariant_cast<DecodedState>(stateSpy.constFirst().at(0)).target.time, 17.0);

    capture.stop();
    QVERIFY(!source.isListening());
    QCOMPARE(source.localPort(), quint16(0));
}

QTEST_GUILESS_MAIN(NetworkAdapterTests)
#include "network_adapter_tests.moc"
