#include "domain/statefield.h"
#include "viewer/lar_projection.h"
#include "viewer/plane/cubemap_catalog.h"
#include "viewer/plane/glb_model_reader.h"
#include "viewer/plane/glb_resource_reader.h"
#include "viewer/plane/glb_texture_reader.h"
#include "viewer/plane/plane_aircraft_scale.h"
#include "viewer/plane/plane_attitude_transform.h"
#include "viewer/plane/plane_orbit_camera.h"
#include "viewer/plane/plane_scene_widget.h"
#include "viewer/plane/plane_surface_projection.h"
#include "viewer/plane/plane_terrain_patch_builder.h"
#include "viewer/plane/plane_view_workspace.h"
#include "viewer/terrain/dted_cell_reader.h"
#include "viewer/terrain/dted_mosaic_sampler.h"
#include "viewer/terrain/dted_water_mask_source.h"

#include "support/dted_fixture.h"

#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QtEndian>
#include <QtMath>
#include <QtTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

QString sourcePath(const QString &relative) {
    return QDir(QStringLiteral(LAR_TEST_SOURCE_DIR)).filePath(relative);
}

bool closeVector(const QVector3D &actual, const QVector3D &expected, float tolerance = 0.0001F) {
    return (actual - expected).length() <= tolerance;
}

QBitArray attitudeFields() {
    QBitArray fields(StateField::Count);
    fields.setBit(StateField::Euler0);
    fields.setBit(StateField::Euler1);
    fields.setBit(StateField::Euler2);
    return fields;
}

LarSceneState representativeSurfaceScene() {
    constexpr double EarthRadius = LarProjection::EarthRadiusMeters;
    const double latitude = qDegreesToRadians(39.0);
    const double longitude = qDegreesToRadians(35.0);
    LarSceneState scene;
    scene.hasScene = true;
    scene.availableFields = QBitArray(StateField::Count);
    for (const int field :
         {StateField::Location0, StateField::Location1, StateField::IrPos0, StateField::IrPos1,
          StateField::IrR, StateField::IzPos0, StateField::IzPos1, StateField::IzTheta1,
          StateField::IzTheta2, StateField::IzR1, StateField::IzR2}) {
        scene.availableFields.setBit(field);
    }
    scene.plane.location[0] = latitude;
    scene.plane.location[1] = longitude;
    scene.target.ir_pos[0] = latitude + 2048.0 / EarthRadius;
    scene.target.ir_pos[1] = longitude + 1024.0 / (EarthRadius * std::cos(latitude));
    scene.target.ir_r = 1024.0;
    scene.target.iz_pos[0] = latitude + 1024.0 / EarthRadius;
    scene.target.iz_pos[1] = longitude - 1536.0 / (EarthRadius * std::cos(latitude));
    scene.target.iz_r1 = 512.0;
    scene.target.iz_r2 = 2048.0;
    scene.target.iz_theta1 = 5.5;
    scene.target.iz_theta2 = 0.5;
    return scene;
}

void appendLittleU32(QByteArray *bytes, quint32 value) {
    const quint32 encoded = qToLittleEndian(value);
    bytes->append(reinterpret_cast<const char *>(&encoded), sizeof(encoded));
}

void appendLittleFloat(QByteArray *bytes, float value) {
    quint32 bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    appendLittleU32(bytes, bits);
}

QByteArray minimalGlb(const std::array<float, 9> &positions, const std::array<float, 9> &normals,
                      bool sparsePosition = false, bool doubleSided = false) {
    QByteArray binary;
    for (const float value : positions)
        appendLittleFloat(&binary, value);
    for (const float value : normals)
        appendLittleFloat(&binary, value);
    for (const quint32 index : {0U, 1U, 2U})
        appendLittleU32(&binary, index);

    QJsonObject positionAccessor{{QStringLiteral("bufferView"), 0},
                                 {QStringLiteral("componentType"), 5126},
                                 {QStringLiteral("count"), 3},
                                 {QStringLiteral("type"), QStringLiteral("VEC3")}};
    if (sparsePosition) {
        positionAccessor.insert(QStringLiteral("sparse"),
                                QJsonObject{{QStringLiteral("count"), 1}});
    }
    const QJsonArray accessors{positionAccessor,
                               QJsonObject{{QStringLiteral("bufferView"), 1},
                                           {QStringLiteral("componentType"), 5126},
                                           {QStringLiteral("count"), 3},
                                           {QStringLiteral("type"), QStringLiteral("VEC3")}},
                               QJsonObject{{QStringLiteral("bufferView"), 2},
                                           {QStringLiteral("componentType"), 5125},
                                           {QStringLiteral("count"), 3},
                                           {QStringLiteral("type"), QStringLiteral("SCALAR")}}};
    const QJsonArray bufferViews{QJsonObject{{QStringLiteral("buffer"), 0},
                                             {QStringLiteral("byteOffset"), 0},
                                             {QStringLiteral("byteLength"), 36}},
                                 QJsonObject{{QStringLiteral("buffer"), 0},
                                             {QStringLiteral("byteOffset"), 36},
                                             {QStringLiteral("byteLength"), 36}},
                                 QJsonObject{{QStringLiteral("buffer"), 0},
                                             {QStringLiteral("byteOffset"), 72},
                                             {QStringLiteral("byteLength"), 12}}};
    const QJsonObject attributes{{QStringLiteral("POSITION"), 0}, {QStringLiteral("NORMAL"), 1}};
    const QJsonObject primitive{{QStringLiteral("attributes"), attributes},
                                {QStringLiteral("indices"), 2},
                                {QStringLiteral("material"), 0}};
    QJsonObject root{
        {QStringLiteral("asset"), QJsonObject{{QStringLiteral("version"), QStringLiteral("2.0")}}},
        {QStringLiteral("scene"), 0},
        {QStringLiteral("scenes"),
         QJsonArray{QJsonObject{{QStringLiteral("nodes"), QJsonArray{0}}}}},
        {QStringLiteral("nodes"), QJsonArray{QJsonObject{{QStringLiteral("mesh"), 0}}}},
        {QStringLiteral("meshes"),
         QJsonArray{QJsonObject{{QStringLiteral("primitives"), QJsonArray{primitive}}}}},
        {QStringLiteral("materials"),
         QJsonArray{QJsonObject{{QStringLiteral("doubleSided"), doubleSided}}}},
        {QStringLiteral("accessors"), accessors},
        {QStringLiteral("bufferViews"), bufferViews},
        {QStringLiteral("buffers"),
         QJsonArray{QJsonObject{{QStringLiteral("byteLength"), binary.size()}}}}};
    QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    while (json.size() % 4 != 0)
        json.append(' ');

    QByteArray glb;
    const quint32 totalLength = static_cast<quint32>(12 + 8 + json.size() + 8 + binary.size());
    appendLittleU32(&glb, 0x46546C67U);
    appendLittleU32(&glb, 2U);
    appendLittleU32(&glb, totalLength);
    appendLittleU32(&glb, static_cast<quint32>(json.size()));
    appendLittleU32(&glb, 0x4E4F534AU);
    glb.append(json);
    appendLittleU32(&glb, static_cast<quint32>(binary.size()));
    appendLittleU32(&glb, 0x004E4942U);
    glb.append(binary);
    return glb;
}

QByteArray minimalGltfJson(const std::array<float, 9> &positions,
                           const std::array<float, 9> &normals, QByteArray *binary) {
    if (binary == nullptr) {
        return {};
    }
    binary->clear();
    for (const float value : positions)
        appendLittleFloat(binary, value);
    for (const float value : normals)
        appendLittleFloat(binary, value);
    for (const quint32 index : {0U, 1U, 2U})
        appendLittleU32(binary, index);

    const QJsonArray accessors{QJsonObject{{QStringLiteral("bufferView"), 0},
                                           {QStringLiteral("componentType"), 5126},
                                           {QStringLiteral("count"), 3},
                                           {QStringLiteral("type"), QStringLiteral("VEC3")}},
                               QJsonObject{{QStringLiteral("bufferView"), 1},
                                           {QStringLiteral("componentType"), 5126},
                                           {QStringLiteral("count"), 3},
                                           {QStringLiteral("type"), QStringLiteral("VEC3")}},
                               QJsonObject{{QStringLiteral("bufferView"), 2},
                                           {QStringLiteral("componentType"), 5125},
                                           {QStringLiteral("count"), 3},
                                           {QStringLiteral("type"), QStringLiteral("SCALAR")}}};
    const QJsonArray bufferViews{QJsonObject{{QStringLiteral("buffer"), 0},
                                             {QStringLiteral("byteOffset"), 0},
                                             {QStringLiteral("byteLength"), 36}},
                                 QJsonObject{{QStringLiteral("buffer"), 0},
                                             {QStringLiteral("byteOffset"), 36},
                                             {QStringLiteral("byteLength"), 36}},
                                 QJsonObject{{QStringLiteral("buffer"), 0},
                                             {QStringLiteral("byteOffset"), 72},
                                             {QStringLiteral("byteLength"), 12}}};
    const QJsonObject primitive{
        {QStringLiteral("attributes"),
         QJsonObject{{QStringLiteral("POSITION"), 0}, {QStringLiteral("NORMAL"), 1}}},
        {QStringLiteral("indices"), 2},
        {QStringLiteral("material"), 0}};
    const QJsonObject root{
        {QStringLiteral("asset"), QJsonObject{{QStringLiteral("version"), QStringLiteral("2.0")}}},
        {QStringLiteral("scene"), 0},
        {QStringLiteral("scenes"),
         QJsonArray{QJsonObject{{QStringLiteral("nodes"), QJsonArray{0}}}}},
        {QStringLiteral("nodes"), QJsonArray{QJsonObject{{QStringLiteral("mesh"), 0}}}},
        {QStringLiteral("meshes"),
         QJsonArray{QJsonObject{{QStringLiteral("primitives"), QJsonArray{primitive}}}}},
        {QStringLiteral("materials"), QJsonArray{QJsonObject{}}},
        {QStringLiteral("accessors"), accessors},
        {QStringLiteral("bufferViews"), bufferViews},
        {QStringLiteral("buffers"),
         QJsonArray{QJsonObject{{QStringLiteral("uri"), QStringLiteral("model.bin")},
                                {QStringLiteral("byteLength"), binary->size()}}}}};
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

GlbModelReadResult readGlbFixture(const QByteArray &bytes) {
    QTemporaryFile file;
    if (!file.open() || file.write(bytes) != bytes.size())
        return {};
    file.close();
    return GlbModelReader::readFile(file.fileName());
}

} // namespace

class PlaneViewTests final : public QObject {
    Q_OBJECT

  private slots:
    void loadsPackagedF16Glb();
    void loadsExternalGltf();
    void rejectsExternalGltfPathEscapeAndAggregateBudget();
    void rejectsRepeatedTextureGpuBudgetAmplification();
    void validatesGlbNumericAndAccessorBoundaries();
    void discoversSixFaceCubemaps();
    void cubemapReloadRevalidatesDiscoveredFiles();
    void appliesAviationYawPitchRollConventions();
    void extremeAttitudeAnglesProduceFiniteOrientation();
    void reportsIncompleteAttitudeWithoutUsingStaleAngles();
    void orbitCameraRemainsAnchoredAndBounded();
    void usesF16AsFifteenMeterSurfaceReference();
    void projectsMetricSurfaceZonesAndTarget();
    void keepsGridAnchoredWhileAircraftMoves();
    void widgetRetainsGroundOriginAcrossPackets();
    void projectsTargetWithoutCompleteLarParameters();
    void wrapsSurfaceProjectionAcrossDateline();
    void surfaceProjectionRequiresAircraftPosition();
    void projectsAltitudeWithoutAircraftGroundPosition();
    void projectsExtremeAltitudeWithinGpuLimits();
    void readsAndValidatesVariableWidthDted0Cells();
    void readsAndValidatesDted1AndDted2Cells();
    void readsAndValidatesDtedWaterMaskPack();
    void addressesAndSamplesDted0WithoutScanning();
    void addressesAndSamplesHighResolutionDtedWithBoundedCache();
    void samplesBathymetryAsSeaLevelWater();
    void buildsBoundedLocalTerrainPatch();
    void buildsFlatDepthColoredWaterPatch();
    void widgetPreparesTerrainAsynchronously();
    void widgetReplacesTerrainSourceForSession();
    void workspaceCyclesSkyboxFromLowerRight();
};

void PlaneViewTests::loadsPackagedF16Glb() {
    const auto result =
        GlbModelReader::readFile(sourcePath(QStringLiteral("assets/models/f16_3.glb")));
    QVERIFY2(result.succeeded(), qPrintable(result.message));
    QVERIFY(result.mesh->vertexCount() > 1000U);
    QCOMPARE(result.mesh->indices.size() % 3U, std::size_t(0));
    QVERIFY(result.mesh->indices.size() > 3000U);
    QVERIFY(result.mesh->indices.size() <= 1500000U);
    QVERIFY(!result.mesh->draws.empty());
    QCOMPARE(result.mesh->textures.size(), std::size_t(3));
    for (const PlaneModelTexture &texture : result.mesh->textures) {
        QVERIFY(!texture.image.isNull());
        QCOMPARE(texture.image.format(), QImage::Format_RGBA8888);
        QVERIFY(texture.image.width() <= 8192);
        QVERIFY(texture.image.height() <= 8192);
    }
    const auto texturedDraws =
        std::count_if(result.mesh->draws.begin(), result.mesh->draws.end(),
                      [](const PlaneModelDrawRange &draw) { return draw.baseColorTexture >= 0; });
    QVERIFY(texturedDraws > 0);
    for (const PlaneModelDrawRange &draw : result.mesh->draws) {
        QVERIFY(draw.baseColorTexture < static_cast<int>(result.mesh->textures.size()));
    }

    float maximumAbsolute = 0.0F;
    for (std::size_t offset = 0;
         offset + PlaneModelVertexStrideFloats - 1U < result.mesh->vertices.size();
         offset += PlaneModelVertexStrideFloats) {
        maximumAbsolute =
            std::max(maximumAbsolute, std::max({std::abs(result.mesh->vertices[offset]),
                                                std::abs(result.mesh->vertices[offset + 1U]),
                                                std::abs(result.mesh->vertices[offset + 2U])}));
        QVERIFY(std::isfinite(result.mesh->vertices[offset + 6U]));
        QVERIFY(std::isfinite(result.mesh->vertices[offset + 7U]));
    }
    QVERIFY(maximumAbsolute <= 1.01F);
    QVERIFY(result.mesh->forwardExtentSceneUnits > 0.0F);
    QVERIFY(result.mesh->forwardExtentSceneUnits <= 2.01F);
    const double metersPerSceneUnit =
        PlaneAircraftScale::metersPerSceneUnit(result.mesh->forwardExtentSceneUnits);
    QVERIFY(std::abs(result.mesh->forwardExtentSceneUnits * metersPerSceneUnit -
                     PlaneAircraftScale::F16LengthMeters) < 0.001);
}

void PlaneViewTests::loadsExternalGltf() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::array<float, 9> positions{-1.0F, 0.0F, -1.0F, 1.0F, 0.0F, -1.0F, 0.0F, 0.0F, 1.0F};
    const std::array<float, 9> normals{0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    QByteArray binary;
    const QByteArray json = minimalGltfJson(positions, normals, &binary);

    QFile binaryFile(directory.filePath(QStringLiteral("model.bin")));
    QVERIFY(binaryFile.open(QIODevice::WriteOnly));
    QCOMPARE(binaryFile.write(binary), binary.size());
    binaryFile.close();
    QFile modelFile(directory.filePath(QStringLiteral("model.gltf")));
    QVERIFY(modelFile.open(QIODevice::WriteOnly));
    QCOMPARE(modelFile.write(json), json.size());
    modelFile.close();

    const auto result = GlbModelReader::readFile(modelFile.fileName());
    QVERIFY2(result.succeeded(), qPrintable(result.message));
    QCOMPARE(result.mesh->vertexCount(), std::size_t(3));
    QCOMPARE(result.mesh->indices.size(), std::size_t(3));
}

void PlaneViewTests::rejectsExternalGltfPathEscapeAndAggregateBudget() {
    const std::array<float, 9> positions{-1.0F, 0.0F, -1.0F, 1.0F, 0.0F, -1.0F, 0.0F, 0.0F, 1.0F};
    const std::array<float, 9> normals{0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    QByteArray binary;
    const QByteArray validJson = minimalGltfJson(positions, normals, &binary);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath(QStringLiteral("package")));
    const QString package = root.filePath(QStringLiteral("package"));

    QFile outside(root.filePath(QStringLiteral("outside.bin")));
    QVERIFY(outside.open(QIODevice::WriteOnly));
    QCOMPARE(outside.write(binary), binary.size());
    outside.close();

    QFile inside(QDir(package).filePath(QStringLiteral("inside.bin")));
    QVERIFY(inside.open(QIODevice::WriteOnly));
    QCOMPARE(inside.write(binary), binary.size());
    inside.close();
    QByteArray resolved;
    QVERIFY(lar::gltf::GlbResourceReader::readUri(QStringLiteral("inside.bin"), package,
                                                  binary.size(), &resolved));
    QCOMPARE(resolved, binary);
    for (const QString &uri :
         {QStringLiteral("../outside.bin"), QStringLiteral("%2e%2e/outside.bin"),
          outside.fileName(), QStringLiteral("inside.bin?version=1"),
          QStringLiteral("inside.bin#payload"), QStringLiteral("file:///etc/passwd")}) {
        QVERIFY2(!lar::gltf::GlbResourceReader::readUri(uri, package, binary.size(), &resolved),
                 qPrintable(uri));
    }
    QString nulUri = QStringLiteral("inside.bin");
    nulUri.append(QChar::Null);
    QVERIFY(!lar::gltf::GlbResourceReader::readUri(nulUri, package, binary.size(), &resolved));

    QJsonObject traversalRoot = QJsonDocument::fromJson(validJson).object();
    traversalRoot.insert(
        QStringLiteral("buffers"),
        QJsonArray{QJsonObject{{QStringLiteral("uri"), QStringLiteral("../outside.bin")},
                               {QStringLiteral("byteLength"), binary.size()}}});
    QFile traversalModel(QDir(package).filePath(QStringLiteral("traversal.gltf")));
    QVERIFY(traversalModel.open(QIODevice::WriteOnly));
    const QByteArray traversalJson = QJsonDocument(traversalRoot).toJson(QJsonDocument::Compact);
    QCOMPARE(traversalModel.write(traversalJson), traversalJson.size());
    traversalModel.close();
    QVERIFY(!GlbModelReader::readFile(traversalModel.fileName()).succeeded());

#ifdef Q_OS_UNIX
    const QString linkedResource = QDir(package).filePath(QStringLiteral("linked.bin"));
    QVERIFY(QFile::link(outside.fileName(), linkedResource));
    QVERIFY(!lar::gltf::GlbResourceReader::readUri(QStringLiteral("linked.bin"), package,
                                                   binary.size(), &resolved));
    traversalRoot.insert(
        QStringLiteral("buffers"),
        QJsonArray{QJsonObject{{QStringLiteral("uri"), QStringLiteral("linked.bin")},
                               {QStringLiteral("byteLength"), binary.size()}}});
    QFile symlinkModel(QDir(package).filePath(QStringLiteral("symlink.gltf")));
    QVERIFY(symlinkModel.open(QIODevice::WriteOnly));
    const QByteArray symlinkJson = QJsonDocument(traversalRoot).toJson(QJsonDocument::Compact);
    QCOMPARE(symlinkModel.write(symlinkJson), symlinkJson.size());
    symlinkModel.close();
    QVERIFY(!GlbModelReader::readFile(symlinkModel.fileName()).succeeded());
#endif

    QJsonObject aggregateRoot = QJsonDocument::fromJson(validJson).object();
    QJsonArray oversizedBuffers;
    for (int index = 0; index < 3; ++index) {
        oversizedBuffers.append(
            QJsonObject{{QStringLiteral("uri"), QStringLiteral("missing-%1.bin").arg(index)},
                        {QStringLiteral("byteLength"), 32 * 1024 * 1024}});
    }
    aggregateRoot.insert(QStringLiteral("buffers"), oversizedBuffers);
    QFile aggregateModel(QDir(package).filePath(QStringLiteral("aggregate.gltf")));
    QVERIFY(aggregateModel.open(QIODevice::WriteOnly));
    const QByteArray aggregateJson = QJsonDocument(aggregateRoot).toJson(QJsonDocument::Compact);
    QCOMPARE(aggregateModel.write(aggregateJson), aggregateJson.size());
    aggregateModel.close();
    QVERIFY(!GlbModelReader::readFile(aggregateModel.fileName()).succeeded());
}

void PlaneViewTests::rejectsRepeatedTextureGpuBudgetAmplification() {
    QImage image(1024, 1024, QImage::Format_RGBA8888);
    image.fill(Qt::white);
    QByteArray png;
    QBuffer output(&png);
    QVERIFY(output.open(QIODevice::WriteOnly));
    QVERIFY(image.save(&output, "PNG"));
    const QString uri =
        QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
    const QJsonArray images{QJsonObject{{QStringLiteral("uri"), uri},
                                        {QStringLiteral("mimeType"), QStringLiteral("image/png")}}};
    QJsonArray textures;
    for (int index = 0; index < 17; ++index)
        textures.append(QJsonObject{{QStringLiteral("source"), 0}});

    PlaneModelMesh mesh;
    QVERIFY(
        !GlbTextureReader::load({}, images, textures, {}, std::vector<QByteArray>{}, {}, &mesh));
    QVERIFY(mesh.textures.empty());
}

void PlaneViewTests::validatesGlbNumericAndAccessorBoundaries() {
    const std::array<float, 9> positions{-1.0F, 0.0F, -1.0F, 1.0F, 0.0F, -1.0F, 0.0F, 0.0F, 1.0F};
    const std::array<float, 9> normals{0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    const auto valid = readGlbFixture(minimalGlb(positions, normals, false, true));
    QVERIFY2(valid.succeeded(), qPrintable(valid.message));
    QCOMPARE(valid.mesh->draws.size(), std::size_t(1));
    QVERIFY(valid.mesh->draws.front().doubleSided);

    auto invalidNormals = normals;
    invalidNormals[0] = std::numeric_limits<float>::quiet_NaN();
    QVERIFY(!readGlbFixture(minimalGlb(positions, invalidNormals)).succeeded());
    QVERIFY(!readGlbFixture(minimalGlb(positions, normals, true)).succeeded());

    const std::array<float, 9> largePositions{2.0e38F, 0.0F,    2.0e38F, 3.0e38F, 0.0F,
                                              2.0e38F, 2.0e38F, 0.0F,    3.0e38F};
    const auto large = readGlbFixture(minimalGlb(largePositions, normals));
    QVERIFY2(large.succeeded(), qPrintable(large.message));
    for (const float value : large.mesh->vertices)
        QVERIFY(std::isfinite(value));
}

void PlaneViewTests::discoversSixFaceCubemaps() {
    CubemapCatalog catalog(sourcePath(QStringLiteral("assets/cubemaps")));
    QCOMPARE(catalog.count(), 15);
    QCOMPARE(catalog.rejectedSetCount(), 1);
    QCOMPARE(catalog.displayName(0), QStringLiteral("Sky 01"));
    QCOMPARE(catalog.displayName(14), QStringLiteral("Sky 15"));

    for (int index = 0; index < catalog.count(); ++index) {
        CubemapFaces faces;
        QString error;
        QVERIFY2(catalog.load(index, &faces, &error), qPrintable(error));
        for (const QImage &face : faces.images) {
            QCOMPARE(face.size(), QSize(1024, 1024));
            QCOMPARE(face.format(), QImage::Format_RGBA8888);
        }
    }
}

void PlaneViewTests::cubemapReloadRevalidatesDiscoveredFiles() {
#ifdef Q_OS_UNIX
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QStringList suffixes{QStringLiteral("lf"), QStringLiteral("rt"), QStringLiteral("up"),
                               QStringLiteral("dn"), QStringLiteral("ft"), QStringLiteral("bk")};
    const QImage face(2, 2, QImage::Format_RGBA8888);
    for (const QString &suffix : suffixes) {
        QVERIFY(face.save(directory.filePath(QStringLiteral("sky_%1.png").arg(suffix))));
    }
    CubemapCatalog catalog(directory.path());
    QCOMPARE(catalog.count(), 1);

    const QString replacement = directory.filePath(QStringLiteral("replacement.png"));
    QVERIFY(face.save(replacement));
    const QString discoveredFace = directory.filePath(QStringLiteral("sky_lf.png"));
    QVERIFY(QFile::remove(discoveredFace));
    QVERIFY(QFile::link(replacement, discoveredFace));
    QVERIFY(QFileInfo(discoveredFace).isSymLink());

    CubemapFaces faces;
    QString error;
    QVERIFY(!catalog.load(0, &faces, &error));
    QVERIFY(!error.isEmpty());
#else
    QSKIP("Symbolic-link behavior is platform specific.");
#endif
}

void PlaneViewTests::appliesAviationYawPitchRollConventions() {
    Plane plane{};
    bool complete = false;
    QQuaternion orientation =
        PlaneAttitudeTransform::orientation(plane, attitudeFields(), &complete);
    QVERIFY(complete);
    QVERIFY(closeVector(orientation.rotatedVector({0.0F, 0.0F, -1.0F}), {0.0F, 0.0F, -1.0F}));

    plane.euler[0] = M_PI_2;
    orientation = PlaneAttitudeTransform::orientation(plane, attitudeFields());
    QVERIFY(closeVector(orientation.rotatedVector({0.0F, 0.0F, -1.0F}), {1.0F, 0.0F, 0.0F}));

    plane = {};
    plane.euler[1] = M_PI_2;
    orientation = PlaneAttitudeTransform::orientation(plane, attitudeFields());
    QVERIFY(closeVector(orientation.rotatedVector({0.0F, 0.0F, -1.0F}), {0.0F, 1.0F, 0.0F}));

    plane = {};
    plane.euler[2] = M_PI_2;
    orientation = PlaneAttitudeTransform::orientation(plane, attitudeFields());
    QVERIFY(closeVector(orientation.rotatedVector({1.0F, 0.0F, 0.0F}), {0.0F, -1.0F, 0.0F}));
}

void PlaneViewTests::extremeAttitudeAnglesProduceFiniteOrientation() {
    Plane plane{};
    plane.euler[0] = std::numeric_limits<double>::max();
    plane.euler[1] = -std::numeric_limits<double>::max();
    plane.euler[2] = std::numeric_limits<double>::max();
    bool complete = false;
    const QQuaternion orientation =
        PlaneAttitudeTransform::orientation(plane, attitudeFields(), &complete);

    QVERIFY(complete);
    QVERIFY(std::isfinite(orientation.scalar()));
    QVERIFY(std::isfinite(orientation.x()));
    QVERIFY(std::isfinite(orientation.y()));
    QVERIFY(std::isfinite(orientation.z()));
    QVERIFY(std::abs(orientation.length() - 1.0F) < 0.0001F);
}

void PlaneViewTests::reportsIncompleteAttitudeWithoutUsingStaleAngles() {
    Plane plane{};
    plane.euler[0] = 1.2;
    QBitArray fields(StateField::Count);
    bool complete = true;
    const QQuaternion orientation = PlaneAttitudeTransform::orientation(plane, fields, &complete);
    QVERIFY(!complete);
    QVERIFY(closeVector(orientation.rotatedVector({0.0F, 0.0F, -1.0F}), {0.0F, 0.0F, -1.0F}));
}

void PlaneViewTests::orbitCameraRemainsAnchoredAndBounded() {
    PlaneOrbitCamera camera;
    QVERIFY(closeVector(camera.position(0.0),
                        {0.0F, static_cast<float>(std::sin(qDegreesToRadians(16.0)) * 6.0),
                         static_cast<float>(std::cos(qDegreesToRadians(16.0)) * 6.0)}));
    const QVector3D eastHeadingPosition = camera.position(M_PI_2);
    QVERIFY(eastHeadingPosition.x() < -5.0F);
    camera.orbit({250.0, -250.0}, {1000, 1000});
    QCOMPARE(camera.azimuthOffsetDegrees(), 45.0);
    QCOMPARE(camera.elevationDegrees(), -14.0);
    camera.orbit({0.0, 100000.0}, {1000, 1000});
    QVERIFY(camera.elevationDegrees() <= 82.0);
    camera.zoom(100000);
    QVERIFY(camera.distance() >= 2.6);
    camera.zoom(-100000);
    QCOMPARE(camera.distance(), 20'000'000.0);
    camera.reset();
    QCOMPARE(camera.azimuthOffsetDegrees(), 0.0);
    QCOMPARE(camera.elevationDegrees(), 16.0);
    QCOMPARE(camera.distance(), 6.0);
    camera.orbit({0.0, -100000.0}, {1000, 1000});
    QCOMPARE(camera.elevationDegrees(), -14.0);
    camera.setGroundConstrained(true);
    QCOMPARE(camera.elevationDegrees(), 3.0);
    QVERIFY(camera.groundConstrained());
    camera.orbit({0.0, -100000.0}, {1000, 1000});
    QCOMPARE(camera.elevationDegrees(), 3.0);
    camera.setGroundConstrained(false);
    camera.orbit({0.0, -100000.0}, {1000, 1000});
    QVERIFY(camera.elevationDegrees() < 3.0);
}

void PlaneViewTests::usesF16AsFifteenMeterSurfaceReference() {
    constexpr float ForwardExtentSceneUnits = 1.875F;
    const double metersPerSceneUnit =
        PlaneAircraftScale::metersPerSceneUnit(ForwardExtentSceneUnits);
    const double renderedLengthMeters = ForwardExtentSceneUnits * metersPerSceneUnit;

    QVERIFY(std::abs(renderedLengthMeters - PlaneAircraftScale::F16LengthMeters) < 0.001);
    QCOMPARE(PlaneAircraftScale::metersPerSceneUnit(0.0F),
             PlaneAircraftScale::DefaultMetersPerSceneUnit);
    QCOMPARE(PlaneAircraftScale::metersPerSceneUnit(std::numeric_limits<float>::quiet_NaN()),
             PlaneAircraftScale::DefaultMetersPerSceneUnit);
}

void PlaneViewTests::projectsMetricSurfaceZonesAndTarget() {
    LarSceneState scene = representativeSurfaceScene();
    scene.availableFields.setBit(StateField::Location2);
    scene.plane.location[2] = 3072.0;
    const PlaneSurfaceState surface = PlaneSurfaceProjection::project(scene, 8.0);
    QVERIFY(surface.geographicAnchorValid);
    QCOMPARE(surface.metersPerSceneUnit, 8.0);
    QCOMPARE(surface.gridSpacingMeters, 256.0);
    QCOMPARE(surface.gridSpacingSceneUnits, 32.0F);
    QVERIFY(surface.inRange.visible);
    QVERIFY(surface.inRange.fullCircle);
    QVERIFY(closeVector(QVector3D(surface.inRange.centerXZ, 0.0F), {128.0F, -256.0F, 0.0F}, 0.01F));
    QCOMPARE(surface.inRange.outerRadius, 128.0F);
    QVERIFY(surface.inZone.visible);
    QVERIFY(!surface.inZone.fullCircle);
    QVERIFY(closeVector(QVector3D(surface.inZone.centerXZ, 0.0F), {-192.0F, -128.0F, 0.0F}, 0.01F));
    QCOMPARE(surface.inZone.innerRadius, 64.0F);
    QCOMPARE(surface.inZone.outerRadius, 256.0F);
    QVERIFY(std::abs(surface.targetMarkerScale - 1.28F) < 0.001F);
    scene.target.iz_r2 *= 2.0;
    const PlaneSurfaceState doubledZone = PlaneSurfaceProjection::project(scene, 8.0);
    QVERIFY(std::abs(doubledZone.targetMarkerScale - surface.targetMarkerScale * 2.0F) < 0.001F);
    QVERIFY(std::abs(surface.inZone.spanRadians - float(2.0 * M_PI - 5.0)) < 0.0001F);
    QVERIFY(surface.targetVisible);
    QCOMPARE(surface.targetXZ, surface.inZone.centerXZ);
    QCOMPARE(surface.surfaceHeight, -384.0F);
    QCOMPARE(surface.surfaceHalfExtent, 19200.0F);
    QCOMPARE(surface.gridPhaseXZ, QVector2D());
    QCOMPARE(surface.groundOriginXZ, QVector2D());
}

void PlaneViewTests::keepsGridAnchoredWhileAircraftMoves() {
    constexpr double EarthRadius = LarProjection::EarthRadiusMeters;
    const GeoCoordinateRadians origin{qDegreesToRadians(39.0), qDegreesToRadians(35.0)};
    LarSceneState scene;
    scene.hasScene = true;
    scene.availableFields = QBitArray(StateField::Count);
    scene.availableFields.setBit(StateField::Location0);
    scene.availableFields.setBit(StateField::Location1);
    scene.plane.location[0] = origin.latitude;
    scene.plane.location[1] = origin.longitude;

    const PlaneSurfaceState initial = PlaneSurfaceProjection::project(scene, 8.0, origin);
    QCOMPARE(initial.gridSpacingMeters, 4.0);
    QCOMPARE(initial.gridSpacingSceneUnits, 0.5F);
    QCOMPARE(initial.gridPhaseXZ, QVector2D());
    QCOMPARE(initial.groundOriginXZ, QVector2D());

    scene.plane.location[0] = origin.latitude + 6.0 / EarthRadius;
    scene.plane.location[1] = origin.longitude + 8.0 / (EarthRadius * std::cos(origin.latitude));
    const PlaneSurfaceState moved = PlaneSurfaceProjection::project(scene, 8.0, origin);
    QVERIFY(closeVector(QVector3D(moved.groundOriginXZ, 0.0F), {-1.0F, 0.75F, 0.0F}, 0.001F));
    QCOMPARE(moved.gridPhaseXZ, moved.groundOriginXZ);

    scene.plane.location[1] = origin.longitude + 28.0 / (EarthRadius * std::cos(origin.latitude));
    const PlaneSurfaceState oneMajorCellLater = PlaneSurfaceProjection::project(scene, 8.0, origin);
    QVERIFY(closeVector(QVector3D(oneMajorCellLater.groundOriginXZ, 0.0F), {-3.5F, 0.75F, 0.0F},
                        0.001F));
    QVERIFY(closeVector(QVector3D(oneMajorCellLater.gridPhaseXZ, 0.0F),
                        QVector3D(moved.gridPhaseXZ, 0.0F), 0.001F));
}

void PlaneViewTests::widgetRetainsGroundOriginAcrossPackets() {
    constexpr double EarthRadius = LarProjection::EarthRadiusMeters;
    PlaneSceneWidget widget(QStringLiteral(LAR_TEST_SOURCE_DIR));
    LarSceneState scene;
    scene.hasScene = true;
    scene.availableFields = QBitArray(StateField::Count);
    scene.availableFields.setBit(StateField::Location0);
    scene.availableFields.setBit(StateField::Location1);
    scene.plane.location[0] = qDegreesToRadians(39.0);
    scene.plane.location[1] = qDegreesToRadians(35.0);
    widget.setSceneState(scene);
    QCOMPARE(widget.surfaceState().gridPhaseXZ, QVector2D());

    scene.plane.location[1] += 8.0 / (EarthRadius * std::cos(scene.plane.location[0]));
    widget.setSceneState(scene);
    QVERIFY(std::abs(widget.surfaceState().gridPhaseXZ.x()) > 0.5F);

    widget.clearScene();
    widget.setSceneState(scene);
    QCOMPARE(widget.surfaceState().gridPhaseXZ, QVector2D());
}

void PlaneViewTests::surfaceProjectionRequiresAircraftPosition() {
    LarSceneState scene = representativeSurfaceScene();
    scene.availableFields.clearBit(StateField::Location1);
    const PlaneSurfaceState surface = PlaneSurfaceProjection::project(scene);
    QVERIFY(!surface.geographicAnchorValid);
    QVERIFY(!surface.inRange.visible);
    QVERIFY(!surface.inZone.visible);
    QVERIFY(!surface.targetVisible);
    QCOMPARE(surface.gridSpacingMeters, 4.0);
}

void PlaneViewTests::projectsAltitudeWithoutAircraftGroundPosition() {
    LarSceneState scene = representativeSurfaceScene();
    scene.availableFields.clearBit(StateField::Location1);
    scene.availableFields.setBit(StateField::Location2);
    scene.plane.location[2] = 3300.0;
    const PlaneSurfaceState surface = PlaneSurfaceProjection::project(scene);

    QVERIFY(!surface.geographicAnchorValid);
    QCOMPARE(surface.surfaceHeight, -440.0F);
    QCOMPARE(-static_cast<double>(surface.surfaceHeight) * surface.metersPerSceneUnit, 3300.0);
    QVERIFY(surface.surfaceHalfExtent > 20'000.0F);
}

void PlaneViewTests::projectsExtremeAltitudeWithinGpuLimits() {
    LarSceneState scene;
    scene.hasScene = true;
    scene.availableFields = QBitArray(StateField::Count);
    scene.availableFields.setBit(StateField::Location2);
    scene.plane.location[2] = std::numeric_limits<double>::max();
    const PlaneSurfaceState surface = PlaneSurfaceProjection::project(scene);

    QVERIFY(std::isfinite(surface.surfaceHeight));
    QVERIFY(std::isfinite(surface.surfaceHalfExtent));
    QVERIFY(std::isfinite(surface.gridSpacingMeters));
    QVERIFY(std::isfinite(surface.gridSpacingSceneUnits));
    QVERIFY(std::abs(surface.surfaceHeight) <= PlaneSurfaceMaximumCoordinate);
    QVERIFY(surface.surfaceHalfExtent <= PlaneSurfaceMaximumCoordinate);
    QVERIFY(surface.gridSpacingSceneUnits <=
            PlaneSurfaceMaximumCoordinate / PlaneSurfaceGridHalfLineCount);
}

void PlaneViewTests::projectsTargetWithoutCompleteLarParameters() {
    LarSceneState scene = representativeSurfaceScene();
    scene.availableFields.clearBit(StateField::IzTheta1);
    scene.availableFields.clearBit(StateField::IzTheta2);
    scene.availableFields.clearBit(StateField::IzR1);
    scene.availableFields.clearBit(StateField::IzR2);
    const PlaneSurfaceState surface = PlaneSurfaceProjection::project(scene);
    QVERIFY(surface.geographicAnchorValid);
    QVERIFY(!surface.inZone.visible);
    QVERIFY(surface.targetVisible);
    QVERIFY(std::abs(surface.targetMarkerScale - surface.inRange.outerRadius * 0.005F) < 0.001F);
    QVERIFY(surface.targetXZ.x() < 0.0F);
    QVERIFY(surface.targetXZ.y() < 0.0F);
}

void PlaneViewTests::readsAndValidatesVariableWidthDted0Cells() {
    const QString standardPath = sourcePath(QStringLiteral("assets/DTED0/e035/n39.dt0"));
    const DtedCellReadResult standard = DtedCellReader::readFile(standardPath);
    QVERIFY2(standard.succeeded(), qPrintable(standard.message));
    QCOMPARE(standard.cell->key, (DtedCellKey{35, 39}));
    QCOMPARE(standard.cell->longitudeSampleCount, 121);
    QCOMPARE(standard.cell->latitudeSampleCount, 121);
    QVERIFY(std::abs(standard.cell->longitudeIntervalDegrees - 1.0 / 120.0) < 1.0e-12);
    QCOMPARE(qRound(*standard.cell->elevation(60, 60)), 1178);

    const DtedCellReadResult polar =
        DtedCellReader::readFile(sourcePath(QStringLiteral("assets/DTED0/w180/s90.dt0")));
    QVERIFY2(polar.succeeded(), qPrintable(polar.message));
    QCOMPARE(polar.cell->key, (DtedCellKey{-180, -90}));
    QCOMPARE(polar.cell->longitudeSampleCount, 21);
    QCOMPARE(polar.cell->latitudeSampleCount, 121);
    QVERIFY(std::abs(polar.cell->longitudeIntervalDegrees - 0.05) < 1.0e-12);

    QFile source(standardPath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QByteArray sourceBytes = source.readAll();
    QByteArray corrupted = sourceBytes;
    QVERIFY(corrupted.size() > 3436);
    corrupted[3436] = static_cast<char>(corrupted.at(3436) ^ 0x01);
    QTemporaryFile temporary;
    QVERIFY(temporary.open());
    QCOMPARE(temporary.write(corrupted), corrupted.size());
    temporary.flush();
    const DtedCellReadResult rejected = DtedCellReader::readFile(temporary.fileName());
    QVERIFY(!rejected.succeeded());
    QVERIFY(rejected.message.contains(QStringLiteral("checksum")));

    QByteArray invalidSpan = sourceBytes;
    invalidSpan.replace(20, 4, QByteArrayLiteral("0600"));
    QTemporaryFile invalidSpanFile;
    QVERIFY(invalidSpanFile.open());
    QCOMPARE(invalidSpanFile.write(invalidSpan), invalidSpan.size());
    invalidSpanFile.flush();
    const DtedCellReadResult invalidSpanResult =
        DtedCellReader::readFile(invalidSpanFile.fileName());
    QVERIFY(!invalidSpanResult.succeeded());
    QVERIFY(invalidSpanResult.message.contains(QStringLiteral("one degree")));
}

void PlaneViewTests::readsAndValidatesDted1AndDted2Cells() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const DtedCellKey level1Key{12, 82};
    QVERIFY(dted_test_fixture::writeCell(directory.path(), DtedLevel::Level1, level1Key, 6, 432));
    const DtedTileSource level1Source(directory.path(), DtedLevel::Level1);
    QVERIFY(level1Source.pathFor(level1Key).endsWith(QStringLiteral("e012/n82.dt1")));
    const DtedCellReadResult level1 = level1Source.load(level1Key);
    QVERIFY2(level1.succeeded(), qPrintable(level1.message));
    QCOMPARE(dtedLevelNumber(level1.cell->level), 1);
    QCOMPARE(level1.cell->longitudeSampleCount, 201);
    QCOMPARE(level1.cell->latitudeSampleCount, 1201);
    QVERIFY(std::abs(level1.cell->longitudeIntervalDegrees - 1.0 / 200.0) < 1.0e-12);
    QVERIFY(std::abs(level1.cell->latitudeIntervalDegrees - 1.0 / 1200.0) < 1.0e-12);
    QCOMPARE(qRound(*level1.cell->elevation(100, 600)), 432);
    QVERIFY(level1.cell->storageBytes() > 400'000U);

    const DtedCellReadResult wrongLevel =
        DtedCellReader::readFile(level1Source.pathFor(level1Key), DtedLevel::Level2);
    QVERIFY(!wrongLevel.succeeded());
    QVERIFY(wrongLevel.message.contains(QStringLiteral("selected level")));

    const DtedCellKey level2Key{-45, -82};
    QVERIFY(dted_test_fixture::writeCell(directory.path(), DtedLevel::Level2, level2Key, 6, -765));
    const DtedTileSource level2Source(directory.path(), DtedLevel::Level2);
    QVERIFY(level2Source.pathFor(level2Key).endsWith(QStringLiteral("w045/s82.dt2")));
    const DtedCellReadResult level2 = level2Source.load(level2Key);
    QVERIFY2(level2.succeeded(), qPrintable(level2.message));
    QCOMPARE(dtedLevelNumber(level2.cell->level), 2);
    QCOMPARE(level2.cell->longitudeSampleCount, 601);
    QCOMPARE(level2.cell->latitudeSampleCount, 3601);
    QVERIFY(std::abs(level2.cell->longitudeIntervalDegrees - 1.0 / 600.0) < 1.0e-12);
    QVERIFY(std::abs(level2.cell->latitudeIntervalDegrees - 1.0 / 3600.0) < 1.0e-12);
    QCOMPARE(qRound(*level2.cell->elevation(300, 1800)), -765);
    QVERIFY(level2.cell->storageBytes() > 4'000'000U);
}

void PlaneViewTests::readsAndValidatesDtedWaterMaskPack() {
    const QString packPath = sourcePath(QStringLiteral("assets/water/dted0_water_mask.bin"));
    const DtedWaterMaskSource source(packPath);
    QVERIFY2(source.isAvailable(), qPrintable(source.initializationError()));

    const DtedWaterMaskReadResult ocean = source.load({-30, 30});
    QVERIFY2(ocean.succeeded(), qPrintable(ocean.message));
    QCOMPARE(ocean.cell->coverage, DtedWaterCoverage::Water);
    QCOMPARE(ocean.cell->water(60, 60), std::optional<bool>(true));

    const DtedWaterMaskReadResult land = source.load({35, 39});
    QVERIFY2(land.succeeded(), qPrintable(land.message));
    QCOMPARE(land.cell->coverage, DtedWaterCoverage::Land);
    QCOMPARE(land.cell->water(60, 60), std::optional<bool>(false));

    const DtedWaterMaskReadResult coast = source.load({25, 35});
    QVERIFY2(coast.succeeded(), qPrintable(coast.message));
    QCOMPARE(coast.cell->coverage, DtedWaterCoverage::Mixed);
    QCOMPARE(coast.cell->water(0, 0), std::optional<bool>(false));
    QCOMPARE(coast.cell->water(60, 60), std::optional<bool>(true));
    QVERIFY(coast.cell->waterAtFraction(0.5, 0.5).has_value());

    QTemporaryFile invalidPack;
    QVERIFY(invalidPack.open());
    QCOMPARE(invalidPack.write(QByteArrayLiteral("not-a-water-mask")), 16);
    invalidPack.flush();
    const DtedWaterMaskSource invalidSource(invalidPack.fileName());
    QVERIFY(!invalidSource.isAvailable());
    QVERIFY(!invalidSource.initializationError().isEmpty());
}

void PlaneViewTests::addressesAndSamplesDted0WithoutScanning() {
    const QString root = sourcePath(QStringLiteral("assets/DTED0"));
    DtedTileSource source(root);
    QVERIFY(source.isAvailable());
    QVERIFY(source.pathFor({35, 39}).endsWith(QStringLiteral("e035/n39.dt0")));
    QVERIFY(source.pathFor({-180, -90}).endsWith(QStringLiteral("w180/s90.dt0")));
    QCOMPARE(*DtedTileSource::keyForRadians(qDegreesToRadians(39.25), qDegreesToRadians(35.75)),
             (DtedCellKey{35, 39}));
    QCOMPARE(*DtedTileSource::keyForRadians(0.0, M_PI), (DtedCellKey{-180, 0}));

    DtedMosaicSampler sampler(DtedTileSource(root), 2U);
    const std::optional<double> center =
        sampler.sampleRadians(qDegreesToRadians(39.5), qDegreesToRadians(35.5));
    QVERIFY2(center.has_value(), qPrintable(sampler.lastError()));
    QVERIFY(std::abs(*center - 1178.0) < 0.1);
    const std::optional<double> sender =
        sampler.sampleRadians(qDegreesToRadians(40.9997), qDegreesToRadians(28.9286));
    QVERIFY2(sender.has_value(), qPrintable(sampler.lastError()));
    QVERIFY(std::abs(*sender - 17.0) < 3.0);
    QVERIFY(sampler.cachedTileCount() <= 2U);
    QVERIFY(!sampler.sampleRadians(qDegreesToRadians(91.0), 0.0));
}

void PlaneViewTests::addressesAndSamplesHighResolutionDtedWithBoundedCache() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const DtedCellKey westCell{-30, 30};
    const DtedCellKey eastCell{-29, 30};
    QVERIFY(dted_test_fixture::writeCell(directory.path(), DtedLevel::Level1, westCell, 1, -2500));
    QVERIFY(dted_test_fixture::writeCell(directory.path(), DtedLevel::Level1, eastCell, 1, -1500));

    const QString mask = sourcePath(QStringLiteral("assets/water/dted0_water_mask.bin"));
    DtedMosaicSampler sampler(DtedTileSource(directory.path(), DtedLevel::Level1),
                              DtedWaterMaskSource(mask), 4U, 3U * 1024U * 1024U);
    const std::optional<DtedSurfaceSample> west =
        sampler.sampleSurfaceRadians(qDegreesToRadians(30.5), qDegreesToRadians(-29.5));
    QVERIFY2(west.has_value(), qPrintable(sampler.lastError()));
    QVERIFY(west->water);
    QCOMPARE(west->elevationMeters, 0.0);
    QCOMPARE(west->waterDepthMeters, 2500.0);

    const std::optional<DtedSurfaceSample> east =
        sampler.sampleSurfaceRadians(qDegreesToRadians(30.5), qDegreesToRadians(-28.5));
    QVERIFY2(east.has_value(), qPrintable(sampler.lastError()));
    QVERIFY(east->water);
    QCOMPARE(east->waterDepthMeters, 1500.0);
    QCOMPARE(sampler.cachedTileCount(), std::size_t(1));
    QVERIFY(sampler.cachedTerrainBytes() <= 3U * 1024U * 1024U);
    QVERIFY(sampler.cachedWaterMaskTileCount() <= 4U);
}

void PlaneViewTests::samplesBathymetryAsSeaLevelWater() {
    const QString root = sourcePath(QStringLiteral("assets/DTED0"));
    const QString mask = sourcePath(QStringLiteral("assets/water/dted0_water_mask.bin"));
    DtedMosaicSampler sampler(DtedTileSource(root), DtedWaterMaskSource(mask), 2U);
    QVERIFY(sampler.waterMaskAvailable());

    const std::optional<DtedSurfaceSample> ocean =
        sampler.sampleSurfaceRadians(qDegreesToRadians(30.5), qDegreesToRadians(-29.5));
    QVERIFY2(ocean.has_value(), qPrintable(sampler.lastError()));
    QVERIFY(ocean->water);
    QCOMPARE(ocean->elevationMeters, 0.0);
    QVERIFY(ocean->waterDepthMeters > 3000.0);
    QVERIFY(ocean->waterDepthMeters < 5000.0);

    const std::optional<DtedSurfaceSample> land =
        sampler.sampleSurfaceRadians(qDegreesToRadians(39.5), qDegreesToRadians(35.5));
    QVERIFY2(land.has_value(), qPrintable(sampler.lastError()));
    QVERIFY(!land->water);
    QCOMPARE(land->waterDepthMeters, 0.0);
    QVERIFY(std::abs(land->elevationMeters - 1178.0) < 0.1);

    const std::optional<DtedSurfaceSample> belowSeaLevelLand =
        sampler.sampleSurfaceRadians(qDegreesToRadians(31.5), qDegreesToRadians(35.5));
    QVERIFY2(belowSeaLevelLand.has_value(), qPrintable(sampler.lastError()));
    QVERIFY(!belowSeaLevelLand->water);
    QVERIFY(belowSeaLevelLand->elevationMeters < -400.0);
    QCOMPARE(belowSeaLevelLand->waterDepthMeters, 0.0);
    QVERIFY(sampler.cachedTileCount() <= 2U);
    QVERIFY(sampler.cachedWaterMaskTileCount() <= 2U);
}

void PlaneViewTests::buildsBoundedLocalTerrainPatch() {
    PlaneTerrainPatchBuilder builder(
        sourcePath(QStringLiteral("assets/DTED0")),
        sourcePath(QStringLiteral("assets/water/dted0_water_mask.bin")));
    PlaneTerrainBuildRequest request;
    request.latitudeRadians = qDegreesToRadians(39.5);
    request.longitudeRadians = qDegreesToRadians(35.5);
    request.halfExtentMeters = 10'000.0;
    request.metersPerSceneUnit = PlaneAircraftScale::DefaultMetersPerSceneUnit;
    request.resolution = 25;
    QString error;
    const PlaneTerrainPatchPtr patch = builder.build(request, &error);
    QVERIFY2(patch != nullptr, qPrintable(error));
    QCOMPARE(patch->vertexCount(), std::size_t(625));
    QCOMPARE(patch->validSampleCount, std::size_t(625));
    QCOMPARE(patch->indices.size(), std::size_t(24 * 24 * 6));
    QVERIFY(patch->minimumElevationMeters <= patch->centerElevationMeters);
    QVERIFY(patch->centerElevationMeters <= patch->maximumElevationMeters);
    QVERIFY(std::abs(patch->centerElevationMeters - 1178.0) < 0.1);
    QCOMPARE(patch->waterSampleCount, std::size_t(0));
    for (std::size_t offset = 0; offset < patch->vertices.size();
         offset += PlaneTerrainVertexStrideFloats) {
        const double length =
            std::hypot(std::hypot(patch->vertices[offset + 3U], patch->vertices[offset + 4U]),
                       patch->vertices[offset + 5U]);
        QVERIFY(std::abs(length - 1.0) < 1.0e-4);
    }

    request.latitudeRadians = qDegreesToRadians(40.9997);
    request.longitudeRadians = qDegreesToRadians(28.9286);
    request.halfExtentMeters = 60'000.0;
    request.resolution = 129;
    const PlaneTerrainPatchPtr maximumPatch = builder.build(request, &error);
    QVERIFY2(maximumPatch != nullptr, qPrintable(error));
    QCOMPARE(maximumPatch->vertexCount(), std::size_t(129 * 129));
    QVERIFY(maximumPatch->indices.size() <= std::size_t(128 * 128 * 6));
    QVERIFY(std::abs(maximumPatch->centerElevationMeters - 17.0) < 3.0);
}

void PlaneViewTests::buildsFlatDepthColoredWaterPatch() {
    PlaneTerrainPatchBuilder builder(
        sourcePath(QStringLiteral("assets/DTED0")),
        sourcePath(QStringLiteral("assets/water/dted0_water_mask.bin")));
    PlaneTerrainBuildRequest request;
    request.latitudeRadians = qDegreesToRadians(30.5);
    request.longitudeRadians = qDegreesToRadians(-29.5);
    request.halfExtentMeters = 10'000.0;
    request.metersPerSceneUnit = PlaneAircraftScale::DefaultMetersPerSceneUnit;
    request.resolution = 25;
    QString error;
    const PlaneTerrainPatchPtr patch = builder.build(request, &error);
    QVERIFY2(patch != nullptr, qPrintable(error));
    QCOMPARE(patch->validSampleCount, std::size_t(625));
    QCOMPARE(patch->waterSampleCount, patch->validSampleCount);
    QCOMPARE(patch->minimumElevationMeters, 0.0);
    QCOMPARE(patch->maximumElevationMeters, 0.0);
    QCOMPARE(patch->centerElevationMeters, 0.0);
    QVERIFY(patch->maximumWaterDepthMeters > 3000.0);
    for (std::size_t offset = 0; offset < patch->vertices.size();
         offset += PlaneTerrainVertexStrideFloats) {
        QCOMPARE(patch->vertices[offset + 1U], 0.0F);
        QCOMPARE(patch->vertices[offset + 6U], 1.0F);
        QVERIFY(patch->vertices[offset + 7U] > 3000.0F);
    }
}

void PlaneViewTests::widgetPreparesTerrainAsynchronously() {
    PlaneSceneWidget widget(QStringLiteral(LAR_TEST_SOURCE_DIR));
    QVERIFY(widget.terrainAvailable());
    QVERIFY(widget.terrainWaterMaskAvailable());
    LarSceneState scene;
    scene.hasScene = true;
    scene.availableFields = QBitArray(StateField::Count);
    scene.availableFields.setBit(StateField::Location0);
    scene.availableFields.setBit(StateField::Location1);
    scene.plane.location[0] = qDegreesToRadians(39.5);
    scene.plane.location[1] = qDegreesToRadians(35.5);
    widget.setSceneState(scene);
    QSignalSpy patchSpy(&widget, &PlaneSceneWidget::terrainPatchChanged);
    widget.setTerrainVisible(true);
    QVERIFY(widget.terrainVisible());
    QTRY_VERIFY_WITH_TIMEOUT(widget.terrainPatchReady(), 5000);
    QVERIFY(!patchSpy.isEmpty());
    QVERIFY(patchSpy.constLast().at(0).toBool());
    widget.setTerrainVisible(false);
    QVERIFY(!widget.terrainVisible());
}

void PlaneViewTests::widgetReplacesTerrainSourceForSession() {
    QTemporaryDir level1Directory;
    QVERIFY(level1Directory.isValid());
    QVERIFY(
        dted_test_fixture::writeCell(level1Directory.path(), DtedLevel::Level1, {35, 39}, 1, 1600));

    PlaneSceneWidget widget(QStringLiteral(LAR_TEST_SOURCE_DIR));
    QCOMPARE(dtedLevelNumber(widget.terrainLevel()), 0);
    QSignalSpy sourceSpy(&widget, &PlaneSceneWidget::terrainSourceChanged);
    QSignalSpy diagnosticSpy(&widget, &PlaneSceneWidget::diagnosticRaised);
    QVERIFY(widget.loadTerrainFromDirectory(level1Directory.path(), DtedLevel::Level1));
    QCOMPARE(dtedLevelNumber(widget.terrainLevel()), 1);
    QCOMPARE(widget.terrainRootDirectory(), QDir::cleanPath(level1Directory.path()));
    QCOMPARE(sourceSpy.size(), 1);
    QVERIFY(!widget.loadTerrainFromDirectory(level1Directory.path(), DtedLevel::Level2));
    QCOMPARE(dtedLevelNumber(widget.terrainLevel()), 1);
    QCOMPARE(sourceSpy.size(), 1);

    QTemporaryDir invalidDirectory;
    QVERIFY(invalidDirectory.isValid());
    QVERIFY(!widget.loadTerrainFromDirectory(invalidDirectory.path(), DtedLevel::Level2));
    QCOMPARE(dtedLevelNumber(widget.terrainLevel()), 1);
    QCOMPARE(widget.terrainRootDirectory(), QDir::cleanPath(level1Directory.path()));
    QVERIFY(!diagnosticSpy.isEmpty());

    LarSceneState scene;
    scene.hasScene = true;
    scene.availableFields = QBitArray(StateField::Count);
    scene.availableFields.setBit(StateField::Location0);
    scene.availableFields.setBit(StateField::Location1);
    scene.plane.location[0] = qDegreesToRadians(39.5);
    scene.plane.location[1] = qDegreesToRadians(35.5);
    widget.setSceneState(scene);
    widget.setTerrainVisible(true);
    QTRY_VERIFY_WITH_TIMEOUT(widget.terrainPatchReady(), 10000);

    QVERIFY(!widget.loadTerrainFromDirectory(invalidDirectory.path(), DtedLevel::Level2));
    QVERIFY(widget.terrainPatchReady());
    QCOMPARE(dtedLevelNumber(widget.terrainLevel()), 1);
    widget.setTerrainVisible(false);
}

void PlaneViewTests::wrapsSurfaceProjectionAcrossDateline() {
    LarSceneState scene = representativeSurfaceScene();
    scene.plane.location[0] = 0.0;
    scene.plane.location[1] = M_PI - 0.0001;
    scene.target.ir_pos[0] = 0.0;
    scene.target.ir_pos[1] = -M_PI + 0.0001;
    scene.target.ir_r = 100.0;
    for (const int field : {StateField::IzPos0, StateField::IzPos1, StateField::IzTheta1,
                            StateField::IzTheta2, StateField::IzR1, StateField::IzR2}) {
        scene.availableFields.clearBit(field);
    }
    const PlaneSurfaceState surface = PlaneSurfaceProjection::project(scene);
    QVERIFY(surface.inRange.visible);
    QVERIFY(surface.targetVisible);
    QVERIFY(surface.inRange.centerXZ.x() > 160.0F);
    QVERIFY(surface.inRange.centerXZ.x() < 180.0F);
    QVERIFY(std::abs(surface.inRange.centerXZ.y()) < 0.0001F);
}

void PlaneViewTests::workspaceCyclesSkyboxFromLowerRight() {
    PlaneViewWorkspace workspace(QStringLiteral(LAR_TEST_SOURCE_DIR));
    workspace.resize(800, 600);
    workspace.show();
    QTest::qWait(1);
    auto *scene = workspace.sceneWidget();
    QVERIFY(scene);
    QCOMPARE(scene->skyboxCount(), 15);
    QCOMPARE(scene->skyboxIndex(), 0);

    auto *changeButton =
        workspace.findChild<QPushButton *>(QStringLiteral("planeChangeSkyboxButton"));
    auto *uploadButton =
        workspace.findChild<QPushButton *>(QStringLiteral("planeUploadModelButton"));
    auto *uploadTerrainButton =
        workspace.findChild<QPushButton *>(QStringLiteral("planeUploadTerrainButton"));
    auto *surfaceButton = workspace.findChild<QPushButton *>(QStringLiteral("planeSurfaceButton"));
    auto *terrainButton = workspace.findChild<QPushButton *>(QStringLiteral("planeTerrainButton"));
    auto *uploadPanel = workspace.findChild<QGroupBox *>(QStringLiteral("planeUploadPanel"));
    auto *uploadHeader = workspace.findChild<QLabel *>(QStringLiteral("planeUploadHeader"));
    auto *displayPanel = workspace.findChild<QFrame *>(QStringLiteral("planeDisplayPanel"));
    QVERIFY(changeButton);
    QVERIFY(uploadButton);
    QVERIFY(uploadTerrainButton);
    QVERIFY(surfaceButton);
    QVERIFY(terrainButton);
    QVERIFY(uploadPanel);
    QVERIFY(uploadHeader);
    QVERIFY(displayPanel);
    QCOMPARE(uploadHeader->text(), QStringLiteral("Upload"));
    QCOMPARE(uploadHeader->alignment(), Qt::AlignCenter);
    QCOMPARE(uploadButton->text(), QStringLiteral("Jet Model"));
    QCOMPARE(uploadTerrainButton->text(), QStringLiteral("DTED Folder"));
    QCOMPARE(terrainButton->text(), QStringLiteral("Terrain"));
    QCOMPARE(surfaceButton->text(), QStringLiteral("Target"));
    QCOMPARE(changeButton->text(), QStringLiteral("Skybox"));
    QCOMPARE(uploadButton->parentWidget(), uploadPanel);
    QCOMPARE(uploadTerrainButton->parentWidget(), uploadPanel);
    QCOMPARE(terrainButton->parentWidget(), displayPanel);
    QCOMPARE(surfaceButton->parentWidget(), displayPanel);
    QCOMPARE(changeButton->parentWidget(), displayPanel);
    QCOMPARE(uploadHeader->parentWidget(), uploadPanel);
    QVERIFY(std::abs(uploadHeader->geometry().center().x() - uploadPanel->rect().center().x()) <=
            1);
    QVERIFY(uploadHeader->geometry().bottom() < uploadButton->geometry().top());
    const auto workspaceButtons = workspace.findChildren<QPushButton *>();
    QCOMPARE(workspaceButtons.size(), 5);
    QVERIFY(uploadPanel->x() <= 20);
    QVERIFY(uploadPanel->y() >= workspace.height() - uploadPanel->height() - 20);
    QVERIFY(displayPanel->x() >= workspace.width() - displayPanel->width() - 20);
    QVERIFY(displayPanel->y() >= workspace.height() - displayPanel->height() - 20);
    QVERIFY(uploadButton->y() < uploadTerrainButton->y());
    QVERIFY(terrainButton->y() < surfaceButton->y());
    QVERIFY(surfaceButton->y() < changeButton->y());
    QVERIFY(terrainButton->isEnabled());
    QVERIFY(terrainButton->isCheckable());
    QVERIFY(surfaceButton->isCheckable());
    QVERIFY(!changeButton->isCheckable());
    bool formatPromptShown = false;
    QTimer::singleShot(0, &workspace, [&formatPromptShown] {
        auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
        if (dialog != nullptr) {
            formatPromptShown = dialog->windowTitle() == QStringLiteral("Select DTED Format") &&
                                dialog->textValue().contains(QStringLiteral("Level 1"));
            dialog->reject();
        }
    });
    QTest::mouseClick(uploadTerrainButton, Qt::LeftButton);
    QVERIFY(formatPromptShown);

    QTemporaryDir level1Directory;
    QVERIFY(level1Directory.isValid());
    QVERIFY(
        dted_test_fixture::writeCell(level1Directory.path(), DtedLevel::Level1, {35, 39}, 1, 1600));
    QVERIFY(scene->loadTerrainFromDirectory(level1Directory.path(), DtedLevel::Level1));
    QCOMPARE(terrainButton->text(), QStringLiteral("Terrain"));
    QVERIFY(terrainButton->toolTip().contains(QStringLiteral("DTED Level 1")));
    QVERIFY(!scene->surfaceVisible());
    QTest::mouseClick(surfaceButton, Qt::LeftButton);
    QVERIFY(scene->surfaceVisible());
    QVERIFY(surfaceButton->isChecked());
    QCOMPARE(surfaceButton->text(), QStringLiteral("Target"));
    QTest::mouseClick(terrainButton, Qt::LeftButton);
    QVERIFY(scene->terrainVisible());
    QVERIFY(terrainButton->isChecked());
    QCOMPARE(terrainButton->text(), QStringLiteral("Terrain"));
    QCOMPARE(scene->skyboxIndex(), 0);
    QTest::mouseClick(changeButton, Qt::LeftButton);
    QCOMPARE(scene->skyboxIndex(), 1);
    QVERIFY(!changeButton->isChecked());
    QVERIFY(scene->surfaceVisible());
}

QTEST_MAIN(PlaneViewTests)
#include "plane_view_tests.moc"
