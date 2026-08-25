#include "tests/support/architecture_test_support.h"

class SessionAdapterTests final : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void persistenceFailurePreservesExistingDestination();
    void sessionAdaptersRoundTripAndClearPartialFailures();
    void sparseIndexCrossesPageBoundaries();
};

void SessionAdapterTests::initTestCase() {
    qRegisterMetaType<SessionSnapshot>("SessionSnapshot");
    qRegisterMetaType<QVector<CapturedPacket>>("QVector<CapturedPacket>");
    qRegisterMetaType<RuntimeStateSource>("RuntimeStateSource");
    qRegisterMetaType<IpAccessPolicy>("IpAccessPolicy");
}

void SessionAdapterTests::persistenceFailurePreservesExistingDestination() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.bin"));
    const QString targetPath = directory.filePath(QStringLiteral("target.lar"));

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write(QByteArrayLiteral("short")), qint64(5));
    source.close();

    QFile target(targetPath);
    QVERIFY(target.open(QIODevice::WriteOnly));
    QCOMPARE(target.write(QByteArrayLiteral("existing-session")), qint64(16));
    target.close();

    const SessionSnapshot snapshot = std::make_shared<FileSessionSnapshot>(sourcePath, 100);
    QtSessionPersistence persistence;
    QString error;
    QVERIFY(!persistence.save(snapshot, targetPath, &error));

    QVERIFY(target.open(QIODevice::ReadOnly));
    QCOMPARE(target.readAll(), QByteArrayLiteral("existing-session"));
    target.close();

    const SessionSnapshot memorySnapshot =
        std::make_shared<FakeSessionSnapshot>(QByteArrayLiteral("substitutable-snapshot"));
    QVERIFY2(persistence.save(memorySnapshot, targetPath, &error), qPrintable(error));
    QVERIFY(target.open(QIODevice::ReadOnly));
    QCOMPARE(target.readAll(), QByteArrayLiteral("substitutable-snapshot"));
}

void SessionAdapterTests::sessionAdaptersRoundTripAndClearPartialFailures() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString snapshotPath = directory.filePath(QStringLiteral("snapshot.lar"));
    const QString finalPath = directory.filePath(QStringLiteral("final.lar"));

    const PacketMapping mapping = timeOnlyMapping();
    Plane plane{};
    Target first{};
    first.time = 1.0;
    Target second{};
    second.time = 2.0;

    LarSessionWriter writer;
    QtSessionPersistence persistence;
    QString error;
    QVERIFY2(writer.begin(mapping.json(), &error), qPrintable(error));
    QVERIFY2(writer.append(sessionTimestamp(0.25), mapping.encode(plane, first), &error),
             qPrintable(error));
    SessionSnapshot snapshot;
    QVERIFY2(writer.createSnapshot(&snapshot, &error), qPrintable(error));
    QVERIFY2(persistence.save(snapshot, snapshotPath, &error), qPrintable(error));
    QVERIFY2(writer.append(sessionTimestamp(0.50), mapping.encode(plane, second), &error),
             qPrintable(error));
    QVERIFY2(writer.createSnapshot(&snapshot, &error), qPrintable(error));
    writer.cancel();
    QVERIFY2(persistence.save(snapshot, finalPath, &error), qPrintable(error));

    LarSessionReader reader;
    QVERIFY2(reader.loadFile(snapshotPath, &error), qPrintable(error));
    QCOMPARE(reader.recordCount(), 1);
    QCOMPARE(reader.duration().milliseconds(), qint64(250));
    SessionStateItem item;
    QVERIFY2(reader.recordAt(0, &item, &error), qPrintable(error));
    QCOMPARE(item.state.target.time, 1.0);

    QVERIFY2(reader.loadFile(finalPath, &error), qPrintable(error));
    QCOMPARE(reader.recordCount(), 2);
    QCOMPARE(reader.duration().milliseconds(), qint64(500));
    QVERIFY2(reader.recordAt(1, &item, &error), qPrintable(error));
    QCOMPARE(item.state.target.time, 2.0);
    QVERIFY(!reader.recordAt(2, &item, &error));
    QVERIFY(error.contains(QStringLiteral("out of range")));

    QFile file(finalPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray corrupt = file.readAll();
    file.close();
    corrupt.append('\0');
    QVERIFY(!reader.loadData(corrupt, &error));
    QVERIFY(!reader.isValid());
    QCOMPARE(reader.recordCount(), 0);

    QVERIFY2(reader.loadFile(finalPath, &error), qPrintable(error));
    QFile truncated(finalPath);
    QVERIFY(truncated.open(QIODevice::WriteOnly | QIODevice::Truncate));
    truncated.close();
    QVERIFY(!reader.recordAt(0, &item, &error));
    QVERIFY(error.contains(QStringLiteral("changed"), Qt::CaseInsensitive));

    const QString limitPath = directory.filePath(QStringLiteral("duration-limit.lar"));
    LarSessionWriter limitWriter;
    QVERIFY2(limitWriter.begin(mapping.json(), &error), qPrintable(error));
    QVERIFY2(limitWriter.append(SessionTimestamp::maximum(), mapping.encode(plane, first), &error),
             qPrintable(error));
    QVERIFY(!SessionTimestamp::fromMilliseconds(
        static_cast<qint64>(lar::session::MaximumDurationMilliseconds) + 1));
    QVERIFY2(limitWriter.createSnapshot(&snapshot, &error), qPrintable(error));
    limitWriter.cancel();
    QVERIFY2(persistence.save(snapshot, limitPath, &error), qPrintable(error));
    QVERIFY2(reader.loadFile(limitPath, &error), qPrintable(error));
    QCOMPARE(reader.duration().milliseconds(),
             static_cast<qint64>(lar::session::MaximumDurationMilliseconds));

    QFile limitFile(limitPath);
    QVERIFY(limitFile.open(QIODevice::ReadOnly));
    QByteArray excessive = limitFile.readAll();
    limitFile.close();
    QDataStream headerStream(excessive);
    headerStream.setByteOrder(QDataStream::LittleEndian);
    headerStream.setVersion(QDataStream::Qt_6_5);
    QVERIFY(headerStream.skipRawData(4) == 4);
    quint32 packetOffset = 0;
    headerStream >> packetOffset;
    QVERIFY(packetOffset < quint32(excessive.size()));
    QDataStream timestampStream(&excessive, QIODevice::ReadWrite);
    timestampStream.setByteOrder(QDataStream::LittleEndian);
    timestampStream.setVersion(QDataStream::Qt_6_5);
    QVERIFY(timestampStream.device()->seek(packetOffset));
    timestampStream << std::numeric_limits<quint64>::max();
    QVERIFY(!reader.loadData(excessive, &error));
    QVERIFY(error.contains(QStringLiteral("365-day limit"), Qt::CaseInsensitive));
}

void SessionAdapterTests::sparseIndexCrossesPageBoundaries() {
    constexpr qint64 RecordCount = 4097;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const PacketMapping mapping = timeOnlyMapping();
    Plane plane{};
    Target target{};
    LarSessionWriter writer;
    QString error;
    QVERIFY2(writer.begin(mapping.json(), &error), qPrintable(error));
    for (qint64 index = 0; index < RecordCount; ++index) {
        target.time = static_cast<double>(index);
        QVERIFY2(writer.append(SessionTimestamp::clampedMilliseconds(index),
                               mapping.encode(plane, target), &error),
                 qPrintable(error));
    }
    QCOMPARE(writer.recordCount(), static_cast<quint64>(RecordCount));

    SessionSnapshot snapshot;
    QVERIFY2(writer.createSnapshot(&snapshot, &error), qPrintable(error));
    const QString path = directory.filePath(QStringLiteral("paged.lar"));
    QVERIFY2(QtSessionPersistence().save(snapshot, path, &error), qPrintable(error));
    writer.cancel();

    LarSessionReader reader;
    QVERIFY2(reader.loadFile(path, &error), qPrintable(error));
    QCOMPARE(reader.recordCount(), RecordCount);
    for (const qint64 index : {qint64(4096), qint64(0), qint64(4095), RecordCount - 1}) {
        SessionStateItem item;
        QVERIFY2(reader.recordAt(index, &item, &error), qPrintable(error));
        QCOMPARE(item.timestamp.milliseconds(), index);
        QCOMPARE(item.state.target.time, static_cast<double>(index));
    }
}

QTEST_GUILESS_MAIN(SessionAdapterTests)
#include "session_adapter_tests.moc"
