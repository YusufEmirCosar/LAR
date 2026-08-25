#include "geojson_source_reader.h"
#include "map_asset_writer.h"
#include "map_mesh_compiler.h"
#include "polygon_triangulator.h"
#include "viewer/map/map_asset_format.h"
#include "viewer/map/map_asset_limits.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>

using namespace lar::map::tool;

namespace {

QByteArray polygonFeature(const QByteArray &coordinates) {
    return QByteArrayLiteral(R"({"type":"FeatureCollection","features":[)"
                             R"({"type":"Feature","properties":{},)"
                             R"("geometry":{"type":"Polygon","coordinates":)") +
           coordinates + QByteArrayLiteral(R"(}}]})");
}

double signedTriangleArea(const PlanarPolygonMesh &mesh) {
    double area = 0.0;
    for (std::size_t offset = 0U; offset < mesh.fillIndices.size(); offset += 3U) {
        const SourceCoordinate &a = mesh.coordinates[mesh.fillIndices[offset]];
        const SourceCoordinate &b = mesh.coordinates[mesh.fillIndices[offset + 1U]];
        const SourceCoordinate &c = mesh.coordinates[mesh.fillIndices[offset + 2U]];
        area += (a.longitudeDegrees * (b.latitudeDegrees - c.latitudeDegrees) +
                 b.longitudeDegrees * (c.latitudeDegrees - a.latitudeDegrees) +
                 c.longitudeDegrees * (a.latitudeDegrees - b.latitudeDegrees)) *
                0.5;
    }
    return area;
}

lar::map::MapMesh minimalMapMesh() {
    lar::map::MapMesh mesh;
    mesh.vertices = {-1.0F, 0.0F, -1.0F, 1.0F, 0.0F, -1.0F, 0.0F, 1.0F, 1.0F};
    mesh.mercatorFillIndices = {0U, 1U, 2U};
    mesh.sphereFillIndices = {0U, 1U, 2U};
    mesh.borderIndices = {0U, 1U};
    return mesh;
}

} // namespace

class MapCompilerTests final : public QObject {
    Q_OBJECT

  private slots:
    void sourceReaderAcceptsSupportedGeometry();
    void sourceReaderRejectsInvalidCoordinates();
    void sourceReaderRejectsOpenRings();
    void triangulatesConcavePolygon();
    void triangulationPreservesHoleArea();
    void triangulationCompletesForMultipleOutOfOrderHoles();
    void compilerIsDeterministic();
    void writerStagesBothFilesBeforeReplacingExistingAsset();
};

void MapCompilerTests::sourceReaderAcceptsSupportedGeometry() {
    const auto result = GeoJsonSourceReader::parse(
        polygonFeature(QByteArrayLiteral("[[[0,0],[4,0],[4,4],[0,4],[0,0]]]")));
    QVERIFY2(result.succeeded(), qPrintable(result.message));
    QCOMPARE(result.map.polygons.size(), std::size_t(1));
    QCOMPARE(result.map.coordinateCount, std::size_t(5));
}

void MapCompilerTests::sourceReaderRejectsInvalidCoordinates() {
    const auto result = GeoJsonSourceReader::parse(
        polygonFeature(QByteArrayLiteral("[[[0,0],[181,0],[4,4],[0,0]]]")));
    QVERIFY(!result.succeeded());
    QCOMPARE(result.error, SourceMapError::InvalidCoordinate);
}

void MapCompilerTests::sourceReaderRejectsOpenRings() {
    const auto result = GeoJsonSourceReader::parse(
        polygonFeature(QByteArrayLiteral("[[[0,0],[4,0],[4,4],[0,4]]]")));
    QVERIFY(!result.succeeded());
    QCOMPARE(result.error, SourceMapError::InvalidSchema);
}

void MapCompilerTests::triangulatesConcavePolygon() {
    SourcePolygon polygon;
    polygon.exterior = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {2.0, 2.0}, {0.0, 4.0}, {0.0, 0.0}};
    const PlanarPolygonMesh mesh = PolygonTriangulator::triangulate(polygon);
    QCOMPARE(mesh.coordinates.size(), std::size_t(5));
    QCOMPARE(mesh.fillIndices.size(), std::size_t(9));
    QVERIFY(std::abs(signedTriangleArea(mesh) - 12.0) < 1.0e-9);
}

void MapCompilerTests::triangulationPreservesHoleArea() {
    SourcePolygon polygon;
    polygon.exterior = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}, {0.0, 0.0}};
    polygon.holes.push_back({{1.0, 1.0}, {1.0, 3.0}, {3.0, 3.0}, {3.0, 1.0}, {1.0, 1.0}});
    const PlanarPolygonMesh mesh = PolygonTriangulator::triangulate(polygon);
    QCOMPARE(mesh.coordinates.size(), std::size_t(8));
    QCOMPARE(mesh.borderIndices.size(), std::size_t(16));
    QVERIFY(std::abs(signedTriangleArea(mesh) - 12.0) < 1.0e-9);
}

void MapCompilerTests::triangulationCompletesForMultipleOutOfOrderHoles() {
    SourcePolygon polygon;
    polygon.exterior = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 6.0}, {0.0, 6.0}, {0.0, 0.0}};
    polygon.holes.push_back({{7.0, 2.0}, {7.0, 4.0}, {9.0, 4.0}, {9.0, 2.0}, {7.0, 2.0}});
    polygon.holes.push_back({{1.0, 2.0}, {1.0, 4.0}, {3.0, 4.0}, {3.0, 2.0}, {1.0, 2.0}});

    const PlanarPolygonMesh mesh = PolygonTriangulator::triangulate(polygon);
    QCOMPARE(mesh.coordinates.size(), std::size_t(12));
    QCOMPARE(mesh.fillIndices.size(), std::size_t(42));
    QCOMPARE(mesh.borderIndices.size(), std::size_t(24));
    QVERIFY(std::abs(signedTriangleArea(mesh) - 52.0) < 1.0e-9);
}

void MapCompilerTests::compilerIsDeterministic() {
    const auto source = GeoJsonSourceReader::parse(
        polygonFeature(QByteArrayLiteral("[[[-4,-4],[4,-4],[4,4],[-4,4],[-4,-4]]]")));
    QVERIFY(source.succeeded());
    const auto first = MapMeshCompiler::compile(source.map);
    const auto second = MapMeshCompiler::compile(source.map);
    QVERIFY2(first.succeeded(), qPrintable(first.message));
    QVERIFY2(second.succeeded(), qPrintable(second.message));
    QCOMPARE(first.mesh.vertices, second.mesh.vertices);
    QCOMPARE(first.mesh.mercatorFillIndices, second.mesh.mercatorFillIndices);
    QCOMPARE(first.mesh.sphereFillIndices, second.mesh.sphereFillIndices);
    QCOMPARE(first.mesh.borderIndices, second.mesh.borderIndices);
}

void MapCompilerTests::writerStagesBothFilesBeforeReplacingExistingAsset() {
    QCOMPARE(lar::map::limits::MaximumPayloadBytes + lar::map::format::HeaderSize,
             static_cast<std::size_t>(lar::map::limits::MaximumAssetBytes));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString assetPath = directory.filePath(QStringLiteral("map.larmap"));
    QFile previousAsset(assetPath);
    QVERIFY(previousAsset.open(QIODevice::WriteOnly));
    QCOMPARE(previousAsset.write(QByteArrayLiteral("previous asset")), qint64(14));
    previousAsset.close();

    const QString unusableManifestPath = directory.filePath(QStringLiteral("manifest.json"));
    QVERIFY(QDir().mkdir(unusableManifestPath));
    QString error;
    QVERIFY(!MapAssetWriter::write(assetPath, unusableManifestPath, minimalMapMesh(), &error));
    QVERIFY(!error.isEmpty());

    QVERIFY(previousAsset.open(QIODevice::ReadOnly));
    QCOMPARE(previousAsset.readAll(), QByteArrayLiteral("previous asset"));
}

QTEST_APPLESS_MAIN(MapCompilerTests)
#include "map_compiler_tests.moc"
