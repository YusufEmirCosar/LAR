#include "viewer/map/map_asset_format.h"
#include "viewer/map/map_asset_reader.h"
#include "viewer/map/map_checksum.h"
#include "viewer/map/map_projection.h"
#include "viewer/map/packaged_map_asset_source.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include <cmath>
#include <cstring>
#include <limits>

using lar::map::MapAssetError;
using lar::map::MapAssetReader;
using lar::map::MapProjection;
using lar::map::PackagedMapAssetSource;

class MapAssetTests final : public QObject {
    Q_OBJECT

  private slots:
    void packagedAssetPassesIntegrityAndFormatValidation();
    void malformedHeaderIsRejectedBeforeAllocation();
    void checksumMismatchIsRejected();
    void zeroChecksumValueIsAcceptedWhenPayloadMatches();
    void invalidPayloadValuesAreRejected();
    void trailingDataIsRejected();
    void manifestMismatchIsRejected();
    void manifestTypeConfusionIsRejected();
    void oversizedManifestIsRejected();
    void packagedPathRejectsSymlinks();
    void packagedDirectoryRejectsSymlinks();
    void mercatorProjectionRoundTrips();
    void longitudeWrappingUsesNearestWorldCopy();
};

QByteArray packagedAssetBytes() {
    QFile file(QStringLiteral(LAR_TEST_MAP_PACKAGE_DIR) + QStringLiteral("/lar_world_map.larmap"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void MapAssetTests::packagedAssetPassesIntegrityAndFormatValidation() {
    const PackagedMapAssetSource source(QStringLiteral(LAR_TEST_MAP_PACKAGE_DIR));
    const auto result = source.load();
    QVERIFY2(result.succeeded(), qPrintable(result.message));
    QCOMPARE(result.mesh->vertexCount(), std::size_t(583762));
    QCOMPARE(result.mesh->mercatorFillIndices.size(), std::size_t(1594269));
    QCOMPARE(result.mesh->sphereFillIndices.size(), std::size_t(1832433));
    QCOMPARE(result.mesh->borderIndices.size(), std::size_t(1088096));
    QVERIFY(result.mesh->byteSize() < 40U * 1024U * 1024U);
}

void MapAssetTests::malformedHeaderIsRejectedBeforeAllocation() {
    QByteArray bytes = packagedAssetBytes();
    QVERIFY(!bytes.isEmpty());
    bytes[0] = 'X';
    const auto badMagic = MapAssetReader::read(QByteArrayView(bytes));
    QCOMPARE(badMagic.error, MapAssetError::Format);
    QVERIFY(!badMagic.mesh);

    bytes = packagedAssetBytes();
    const std::uint64_t excessiveCount =
        qToLittleEndian<std::uint64_t>(std::numeric_limits<std::uint64_t>::max());
    std::memcpy(bytes.data() + lar::map::format::VertexCountOffset, &excessiveCount,
                sizeof(excessiveCount));
    const auto badCount = MapAssetReader::read(QByteArrayView(bytes));
    QCOMPARE(badCount.error, MapAssetError::Limits);
    QVERIFY(!badCount.mesh);
}

void MapAssetTests::checksumMismatchIsRejected() {
    QByteArray bytes = packagedAssetBytes();
    QVERIFY(bytes.size() > static_cast<qsizetype>(lar::map::format::HeaderSize));
    bytes[bytes.size() - 1] = static_cast<char>(bytes.at(bytes.size() - 1) ^ 0x01);
    const auto result = MapAssetReader::read(QByteArrayView(bytes));
    QCOMPARE(result.error, MapAssetError::Integrity);
    QVERIFY(!result.mesh);
}

void MapAssetTests::zeroChecksumValueIsAcceptedWhenPayloadMatches() {
    const QByteArray payload =
        QByteArray::fromHex("eda1ad4300000000000080bf0000803f00000000000080bf000000000000803f"
                            "2ff56aa600000000010000000200000000000000010000000200000000000000"
                            "01000000");
    QCOMPARE(payload.size(), 68);
    QCOMPARE(
        lar::map::MapChecksum::crc32(reinterpret_cast<const unsigned char *>(payload.constData()),
                                     static_cast<std::size_t>(payload.size())),
        std::uint32_t(0));

    QByteArray bytes(static_cast<qsizetype>(lar::map::format::HeaderSize), '\0');
    std::memcpy(bytes.data() + lar::map::format::MagicOffset, lar::map::format::Magic.data(),
                lar::map::format::Magic.size());
    const auto writeU32 = [&bytes](std::size_t offset, std::uint32_t value) {
        const std::uint32_t encoded = qToLittleEndian(value);
        std::memcpy(bytes.data() + static_cast<qsizetype>(offset), &encoded, sizeof(encoded));
    };
    const auto writeU64 = [&bytes](std::size_t offset, std::uint64_t value) {
        const std::uint64_t encoded = qToLittleEndian(value);
        std::memcpy(bytes.data() + static_cast<qsizetype>(offset), &encoded, sizeof(encoded));
    };
    writeU32(lar::map::format::VersionOffset, lar::map::format::Version);
    writeU32(lar::map::format::HeaderSizeOffset, lar::map::format::HeaderSize);
    writeU32(lar::map::format::FlagsOffset, lar::map::format::SupportedFlags);
    writeU64(lar::map::format::VertexCountOffset, 3U);
    writeU64(lar::map::format::MercatorIndexCountOffset, 3U);
    writeU64(lar::map::format::SphereIndexCountOffset, 3U);
    writeU64(lar::map::format::BorderIndexCountOffset, 2U);
    writeU64(lar::map::format::PayloadSizeOffset, static_cast<std::uint64_t>(payload.size()));
    writeU32(lar::map::format::PayloadCrcOffset, 0U);
    writeU32(lar::map::format::ReservedOffset, 0U);
    bytes.append(payload);

    const auto result = MapAssetReader::read(QByteArrayView(bytes));
    QVERIFY2(result.succeeded(), qPrintable(result.message));
    QCOMPARE(result.mesh->vertexCount(), std::size_t(3));
}

void MapAssetTests::invalidPayloadValuesAreRejected() {
    QByteArray bytes = packagedAssetBytes();
    QVERIFY(bytes.size() > static_cast<qsizetype>(lar::map::format::HeaderSize + sizeof(float)));
    const float invalid = std::numeric_limits<float>::quiet_NaN();
    std::memcpy(bytes.data() + lar::map::format::HeaderSize, &invalid, sizeof(invalid));
    const auto *payload =
        reinterpret_cast<const unsigned char *>(bytes.constData() + lar::map::format::HeaderSize);
    const std::size_t payloadSize =
        static_cast<std::size_t>(bytes.size()) - lar::map::format::HeaderSize;
    const std::uint32_t encodedCrc =
        qToLittleEndian(lar::map::MapChecksum::crc32(payload, payloadSize));
    std::memcpy(bytes.data() + lar::map::format::PayloadCrcOffset, &encodedCrc, sizeof(encodedCrc));

    const auto result = MapAssetReader::read(QByteArrayView(bytes));
    QCOMPARE(result.error, MapAssetError::Format);
    QVERIFY(!result.mesh);
}

void MapAssetTests::trailingDataIsRejected() {
    QByteArray bytes = packagedAssetBytes();
    bytes.append('\0');
    const auto result = MapAssetReader::read(QByteArrayView(bytes));
    QCOMPARE(result.error, MapAssetError::Format);
    QVERIFY(!result.mesh);
}

void MapAssetTests::manifestMismatchIsRejected() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QFile::copy(QStringLiteral(LAR_TEST_MAP_PACKAGE_DIR) +
                            QStringLiteral("/lar_world_map.larmap"),
                        directory.filePath(QStringLiteral("lar_world_map.larmap"))));
    QFile manifest(directory.filePath(QStringLiteral("lar_world_map.manifest.json")));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    QVERIFY(
        manifest.write(QByteArrayLiteral(
            "{\"formatVersion\":1,\"bytes\":1,"
            "\"sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\"}")) >
        0);
    manifest.close();

    const PackagedMapAssetSource source(directory.path());
    const auto result = source.load();
    QCOMPARE(result.error, MapAssetError::Integrity);
    QVERIFY(!result.mesh);
}

void MapAssetTests::oversizedManifestIsRejected() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QFile::copy(QStringLiteral(LAR_TEST_MAP_PACKAGE_DIR) +
                            QStringLiteral("/lar_world_map.larmap"),
                        directory.filePath(QStringLiteral("lar_world_map.larmap"))));
    QFile manifest(directory.filePath(QStringLiteral("lar_world_map.manifest.json")));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    QCOMPARE(manifest.write(QByteArray(4097, 'x')), qint64(4097));
    manifest.close();

    const PackagedMapAssetSource source(directory.path());
    const auto result = source.load();
    QCOMPARE(result.error, MapAssetError::Integrity);
    QVERIFY(!result.mesh);
}

void MapAssetTests::manifestTypeConfusionIsRejected() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourceDirectory = QStringLiteral(LAR_TEST_MAP_PACKAGE_DIR);
    const QString assetName = QStringLiteral("lar_world_map.larmap");
    const QString manifestName = QStringLiteral("lar_world_map.manifest.json");
    QVERIFY(
        QFile::copy(sourceDirectory + QLatin1Char('/') + assetName, directory.filePath(assetName)));

    QFile sourceManifest(sourceDirectory + QLatin1Char('/') + manifestName);
    QVERIFY(sourceManifest.open(QIODevice::ReadOnly));
    QJsonObject manifest = QJsonDocument::fromJson(sourceManifest.readAll()).object();
    QVERIFY(!manifest.isEmpty());
    manifest.insert(QStringLiteral("bytes"),
                    QString::number(QFileInfo(directory.filePath(assetName)).size()));

    QFile destinationManifest(directory.filePath(manifestName));
    QVERIFY(destinationManifest.open(QIODevice::WriteOnly));
    QVERIFY(destinationManifest.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact)) > 0);
    destinationManifest.close();

    const PackagedMapAssetSource source(directory.path());
    const auto result = source.load();
    QCOMPARE(result.error, MapAssetError::Integrity);
    QVERIFY(!result.mesh);
}

void MapAssetTests::packagedPathRejectsSymlinks() {
#ifdef Q_OS_UNIX
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString package = QStringLiteral(LAR_TEST_MAP_PACKAGE_DIR);
    const QString assetName = QStringLiteral("lar_world_map.larmap");
    const QString manifestName = QStringLiteral("lar_world_map.manifest.json");
    QVERIFY(QFile::link(package + QLatin1Char('/') + assetName, directory.filePath(assetName)));
    QVERIFY(QFileInfo(directory.filePath(assetName)).isSymLink());
    QVERIFY(
        QFile::copy(package + QLatin1Char('/') + manifestName, directory.filePath(manifestName)));

    const PackagedMapAssetSource source(directory.path());
    const auto result = source.load();
    QCOMPARE(result.error, MapAssetError::Integrity);
    QVERIFY(!result.mesh);
#else
    QSKIP("Symbolic-link behavior is platform specific.");
#endif
}

void MapAssetTests::packagedDirectoryRejectsSymlinks() {
#ifdef Q_OS_UNIX
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString linkPath = directory.filePath(QStringLiteral("map-package"));
    QVERIFY(QFile::link(QStringLiteral(LAR_TEST_MAP_PACKAGE_DIR), linkPath));
    QVERIFY(QFileInfo(linkPath).isSymLink());

    const PackagedMapAssetSource source(linkPath);
    const auto result = source.load();
    QCOMPARE(result.error, MapAssetError::Io);
    QVERIFY(!result.mesh);
#else
    QSKIP("Symbolic-link behavior is platform specific.");
#endif
}

void MapAssetTests::mercatorProjectionRoundTrips() {
    for (const double latitude : {-85.0, -45.0, 0.0, 45.0, 85.0}) {
        const double projected = MapProjection::projectLatitude(latitude);
        const double roundTrip = MapProjection::unprojectLatitude(projected);
        QVERIFY(std::abs(roundTrip - latitude) < 1.0e-10);
    }
}

void MapAssetTests::longitudeWrappingUsesNearestWorldCopy() {
    QCOMPARE(MapProjection::wrapLongitude(181.0), -179.0);
    QCOMPARE(MapProjection::wrapLongitude(-181.0), 179.0);
    QCOMPARE(MapProjection::unwrapLongitude(-179.0, 179.0), 181.0);
    QCOMPARE(MapProjection::unwrapLongitude(179.0, -179.0), -181.0);
}

QTEST_APPLESS_MAIN(MapAssetTests)

#include "map_asset_tests.moc"
