#include "domain/statefield.h"
#include "infrastructure/mapping/json_mapping_repository.h"
#include "infrastructure/mapping/mapped_packet_decoder.h"
#include "infrastructure/session/lar_session_reader.h"
#include "infrastructure/session/lar_session_writer.h"
#include "infrastructure/session/qt_session_persistence.h"
#include "testsender/scenarios.h"
#include "viewer/map/packaged_map_asset_source.h"
#include "viewer/viewport/lar_zone_mesh_builder.h"

#include <QBitArray>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace {

struct BenchmarkSummary final {
    double median = 0.0;
    double p95 = 0.0;
};

BenchmarkSummary measure(int samples, int operationsPerSample,
                         const std::function<void()> &operation) {
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(samples));
    for (int sample = 0; sample < samples; ++sample) {
        QElapsedTimer timer;
        timer.start();
        for (int operationIndex = 0; operationIndex < operationsPerSample; ++operationIndex)
            operation();
        values.push_back(static_cast<double>(timer.nsecsElapsed()) /
                         static_cast<double>(operationsPerSample));
    }
    std::sort(values.begin(), values.end());
    const std::size_t medianIndex = values.size() / 2U;
    const std::size_t p95Index = std::min(
        values.size() - 1U,
        static_cast<std::size_t>(std::ceil(static_cast<double>(values.size()) * 0.95)) - 1U);
    return {values[medianIndex], values[p95Index]};
}

QJsonObject metric(const BenchmarkSummary &summary, const QString &unit, double divisor = 1.0) {
    return {{QStringLiteral("median"), summary.median / divisor},
            {QStringLiteral("p95"), summary.p95 / divisor},
            {QStringLiteral("unit"), unit}};
}

bool writeReport(const QString &path, const QJsonDocument &document, QString *error) {
    const QByteArray bytes = document.toJson(QJsonDocument::Indented);
    if (path.isEmpty()) {
        QFile output;
        if (!output.open(stdout, QIODevice::WriteOnly) || output.write(bytes) != bytes.size()) {
            if (error)
                *error = QStringLiteral("Cannot write benchmark JSON to stdout");
            return false;
        }
        return true;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("lar-performance-benchmarks"));
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption outputOption({QStringLiteral("o"), QStringLiteral("output")},
                                          QStringLiteral("Write JSON to this path."),
                                          QStringLiteral("path"));
    parser.addOption(outputOption);
    parser.process(application);

    const QString mappingPath =
        QStringLiteral(LAR_TEST_SOURCE_DIR) + QStringLiteral("/maps/full-state.json");
    PacketMapping mapping;
    QString error;
    if (!JsonMappingRepository().loadFile(mappingPath, &mapping, &error)) {
        qCritical().noquote() << error;
        return 2;
    }

    Plane plane{};
    Target target{};
    TestSenderScenarios::initialize(QStringLiteral("mixed-high-dynamics"), &plane, &target);
    TestSenderScenarios::update(QStringLiteral("mixed-high-dynamics"), 40, 4.0, &plane, &target);
    const QByteArray packet = mapping.encode(plane, target);

    MappedPacketDecoder decoder(mapping);
    DecodedState decoded;
    volatile double numericSink = 0.0;
    const BenchmarkSummary decodeSummary = measure(25, 2000, [&] {
        if (!decoder.decode(packet, &decoded, &error))
            return;
        numericSink = numericSink + decoded.target.time;
    });

    QTemporaryDir directory;
    if (!directory.isValid()) {
        qCritical() << "Cannot create benchmark directory";
        return 2;
    }
    const QString sessionPath = directory.filePath(QStringLiteral("benchmark.lar"));
    LarSessionWriter writer;
    if (!writer.begin(mapping.json(), &error)) {
        qCritical().noquote() << error;
        return 2;
    }
    for (int index = 0; index < 1000; ++index) {
        const auto timestamp = SessionTimestamp::fromMilliseconds(qint64(index) * 10);
        if (!timestamp || !writer.append(*timestamp, packet, &error)) {
            qCritical().noquote() << error;
            return 2;
        }
    }
    SessionSnapshot snapshot;
    if (!writer.createSnapshot(&snapshot, &error) ||
        !QtSessionPersistence().save(snapshot, sessionPath, &error)) {
        qCritical().noquote() << error;
        return 2;
    }

    const BenchmarkSummary snapshotSummary = measure(25, 100, [&] {
        SessionSnapshot measuredSnapshot;
        if (writer.createSnapshot(&measuredSnapshot, &error))
            numericSink = numericSink + static_cast<double>(measuredSnapshot.use_count());
    });

    LarSessionReader reader;
    const BenchmarkSummary sessionLoadSummary = measure(15, 1, [&] {
        reader.close();
        if (reader.loadFile(sessionPath, &error))
            numericSink = numericSink + static_cast<double>(reader.recordCount());
    });

    lar::map::PackagedMapAssetSource mapSource(QStringLiteral(LAR_TEST_MAP_PACKAGE_DIR));
    const BenchmarkSummary mapLoadSummary = measure(11, 1, [&] {
        const lar::map::MapAssetReadResult result = mapSource.load();
        if (result.succeeded())
            numericSink = numericSink + static_cast<double>(result.mesh->vertexCount());
    });

    QBitArray fields(StateField::Count);
    for (int field = StateField::IzPos0; field <= StateField::IrR; ++field)
        fields.setBit(field);
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Mercator);
    LarZoneMeshBuilder meshBuilder;
    const BenchmarkSummary meshSummary = measure(15, 10, [&] {
        const LarZoneMesh mesh = meshBuilder.build(target, fields, camera, 1600, 900);
        numericSink = numericSink + static_cast<double>(mesh.indices.size());
    });

    const QJsonObject metrics{
        {QStringLiteral("packet_decode"), metric(decodeSummary, QStringLiteral("ns"))},
        {QStringLiteral("snapshot_prepare"), metric(snapshotSummary, QStringLiteral("ns"))},
        {QStringLiteral("session_load"), metric(sessionLoadSummary, QStringLiteral("us"), 1000.0)},
        {QStringLiteral("map_load"), metric(mapLoadSummary, QStringLiteral("us"), 1000.0)},
        {QStringLiteral("mesh_build"), metric(meshSummary, QStringLiteral("us"), 1000.0)},
    };
    const QJsonDocument report(QJsonObject{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("metrics"), metrics},
        {QStringLiteral("resource_invariants"),
         QJsonObject{{QStringLiteral("mesh_vertices_max"),
                      static_cast<qint64>(LarZoneMeshBuilder::MaximumVertexCount)},
                     {QStringLiteral("mesh_indices_max"),
                      static_cast<qint64>(LarZoneMeshBuilder::MaximumIndexCount)}}},
    });
    if (!writeReport(parser.value(outputOption), report, &error)) {
        qCritical().noquote() << error;
        return 2;
    }
    return numericSink < 0.0 ? 3 : 0;
}
