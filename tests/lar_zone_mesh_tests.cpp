#include "domain/statefield.h"
#include "viewer/lar_geodesic_geometry.h"
#include "viewer/lar_geometry_builder.h"
#include "viewer/map/map_camera.h"
#include "viewer/viewport/geodesic_zone_sampler.h"
#include "viewer/viewport/lar_parametric_zone_gpu_layer.h"
#include "viewer/viewport/lar_zone_input_validator.h"
#include "viewer/viewport/lar_zone_mesh_builder.h"

#include <QBitArray>
#include <QtMath>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

class LarZoneMeshTests final : public QObject {
    Q_OBJECT

  private slots:
    void inRangeBoundaryRemainsGeodesic();
    void inRangeUsesIndependentIrCenter();
    void mercatorCutoffClipsAndKeepsZone();
    void polarMercatorCirclesFillWithoutSeamSpikes();
    void polarSphereCirclesUseBoundedSurfaceTessellation();
    void hostileValuesAreRejectedWithinBudgets();
    void extremeAnglesRemainFiniteAndBounded();
    void annularSectorProducesBoundedFillAndOutline();
    void polarOverlayBoundsSelectWorldCopies();
    void validatorSeparatesAvailabilityFromInvalidInput();
    void samplerIsDeterministicFiniteAndBounded();
    void extremeZoomUsesPixelAccurateRelativeFallback();
};

QBitArray available(std::initializer_list<int> fields) {
    QBitArray result(StateField::Count);
    for (const int field : fields) {
        result.setBit(field);
    }
    return result;
}

double angularDistance(double latitudeA, double longitudeA, double latitudeB, double longitudeB) {
    return std::acos(std::clamp(std::sin(latitudeA) * std::sin(latitudeB) +
                                    std::cos(latitudeA) * std::cos(latitudeB) *
                                        std::cos(longitudeB - longitudeA),
                                -1.0, 1.0));
}

GeoCoordinateRadians meshCoordinate(const LarZoneMesh &mesh, std::uint32_t vertexIndex) {
    const std::size_t offset = static_cast<std::size_t>(vertexIndex) * 3U;
    double latitude = static_cast<double>(mesh.vertices[offset + 1U]);
    double longitude = static_cast<double>(mesh.vertices[offset]);
    if (mesh.coordinateSpace == LarZoneCoordinateSpace::MercatorCameraRelative) {
        longitude += mesh.coordinateOrigin.x();
    } else if (mesh.coordinateSpace == LarZoneCoordinateSpace::SphereCameraRelative) {
        longitude += mesh.coordinateOrigin.x();
        latitude += mesh.coordinateOrigin.y();
    }
    return {qDegreesToRadians(latitude), qDegreesToRadians(longitude)};
}

double maximumTriangleEdgeRadians(const LarZoneMesh &mesh, const LarZoneDrawRange &range) {
    double maximum = 0.0;
    const std::size_t end = range.firstIndex + range.indexCount;
    for (std::size_t offset = range.firstIndex; offset + 2U < end; offset += 3U) {
        const GeoCoordinateRadians points[] = {meshCoordinate(mesh, mesh.indices[offset]),
                                               meshCoordinate(mesh, mesh.indices[offset + 1U]),
                                               meshCoordinate(mesh, mesh.indices[offset + 2U])};
        for (int edge = 0; edge < 3; ++edge) {
            const GeoCoordinateRadians &first = points[edge];
            const GeoCoordinateRadians &second = points[(edge + 1) % 3];
            maximum = std::max(maximum, angularDistance(first.latitude, first.longitude,
                                                        second.latitude, second.longitude));
        }
    }
    return maximum;
}

bool hasMercatorLongitudeSpike(const LarZoneMesh &mesh, const LarZoneDrawRange &range) {
    const std::size_t end = range.firstIndex + range.indexCount;
    for (std::size_t offset = range.firstIndex; offset + 2U < end; offset += 3U) {
        double longitude[3]{};
        for (std::size_t vertex = 0; vertex < 3U; ++vertex) {
            longitude[vertex] =
                mesh.vertices[static_cast<std::size_t>(mesh.indices[offset + vertex]) * 3U];
        }
        if (std::abs(longitude[0] - longitude[1]) > 180.001 ||
            std::abs(longitude[1] - longitude[2]) > 180.001 ||
            std::abs(longitude[2] - longitude[0]) > 180.001) {
            return true;
        }
    }
    return false;
}

double pointToSegmentDistance(const QPointF &point, const QPointF &first, const QPointF &second) {
    const QPointF edge = second - first;
    const double lengthSquared = edge.x() * edge.x() + edge.y() * edge.y();
    if (lengthSquared <= 0.0)
        return std::hypot(point.x() - first.x(), point.y() - first.y());
    const QPointF offset = point - first;
    const double fraction =
        std::clamp((offset.x() * edge.x() + offset.y() * edge.y()) / lengthSquared, 0.0, 1.0);
    const QPointF nearest = first + edge * fraction;
    return std::hypot(point.x() - nearest.x(), point.y() - nearest.y());
}

void LarZoneMeshTests::inRangeBoundaryRemainsGeodesic() {
    Target target{};
    target.ir_pos[0] = 0.8;
    target.ir_pos[1] = 2.9;
    target.ir_r = 750000.0;
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Mercator);
    camera.setMercatorZoom(3.0F, 1000, 700);

    const LarZoneMesh mesh = LarZoneMeshBuilder().build(
        target, available({StateField::IrPos0, StateField::IrPos1, StateField::IrR}), camera, 1000,
        700);
    QVERIFY(!mesh.empty());
    QVERIFY(!mesh.mercatorGeometryClipped);
    QVERIFY(!mesh.inputRejected);

    QVERIFY(mesh.inRangeLines.indexCount >= 64U * 2U);
    const std::size_t lineEnd = mesh.inRangeLines.firstIndex + mesh.inRangeLines.indexCount;
    for (std::size_t index = mesh.inRangeLines.firstIndex; index < lineEnd; ++index) {
        const GeoCoordinateRadians point = meshCoordinate(mesh, mesh.indices[index]);
        const double distance =
            angularDistance(target.ir_pos[0], target.ir_pos[1], point.latitude, point.longitude) *
            LarGeodesicGeometry::EarthRadiusMeters;
        QVERIFY(std::abs(distance - target.ir_r) < 1.0);
    }
}

void LarZoneMeshTests::inRangeUsesIndependentIrCenter() {
    Target target{};
    target.ir_pos[0] = qDegreesToRadians(41.06);
    target.ir_pos[1] = qDegreesToRadians(29.67);
    target.ir_r = 26000.0;
    target.iz_pos[0] = qDegreesToRadians(56.53);
    target.iz_pos[1] = qDegreesToRadians(-80.0);
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Mercator);

    const LarZoneMesh mesh = LarZoneMeshBuilder().build(
        target, available({StateField::IrPos0, StateField::IrPos1, StateField::IrR}), camera, 1000,
        700);

    QVERIFY(!mesh.inputRejected);
    QVERIFY(mesh.inRangeFill.indexCount > 0U);
    const GeoCoordinateRadians center =
        meshCoordinate(mesh, mesh.indices[mesh.inRangeFill.firstIndex]);
    QVERIFY(angularDistance(center.latitude, center.longitude, target.ir_pos[0], target.ir_pos[1]) <
            1.0e-6);
    QVERIFY(angularDistance(center.latitude, center.longitude, target.iz_pos[0], target.iz_pos[1]) >
            1.0);
}

void LarZoneMeshTests::mercatorCutoffClipsAndKeepsZone() {
    Target target{};
    target.ir_pos[0] = qDegreesToRadians(84.5);
    target.ir_pos[1] = 0.0;
    target.ir_r = 500000.0;
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Mercator);

    const LarZoneMesh mesh = LarZoneMeshBuilder().build(
        target, available({StateField::IrPos0, StateField::IrPos1, StateField::IrR}), camera, 800,
        600);
    QVERIFY(mesh.mercatorGeometryClipped);
    QVERIFY(!mesh.inputRejected);
    QVERIFY(mesh.inRangeFill.indexCount > 0U);
    QVERIFY(mesh.inRangeLines.indexCount > 0U);
}

void LarZoneMeshTests::polarMercatorCirclesFillWithoutSeamSpikes() {
    Target target{};
    target.ir_pos[0] = qDegreesToRadians(80.0);
    target.ir_pos[1] = 0.0;
    target.ir_r = 2'000'000.0;
    target.iz_pos[0] = qDegreesToRadians(78.0);
    target.iz_pos[1] = qDegreesToRadians(20.0);
    target.iz_theta1 = 0.0;
    target.iz_theta2 = 0.0;
    target.iz_r1 = 0.0;
    target.iz_r2 = 2'300'000.0;
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Mercator);

    const LarZoneMesh mesh =
        LarZoneMeshBuilder().build(target, QBitArray(StateField::Count, true), camera, 1200, 800);

    QVERIFY(!mesh.empty());
    QVERIFY(!mesh.inputRejected);
    QVERIFY(mesh.mercatorGeometryClipped);
    QVERIFY(mesh.inRangeFill.indexCount > 64U * 3U);
    QVERIFY(mesh.inZoneFill.indexCount > 64U * 3U);
    QVERIFY(!hasMercatorLongitudeSpike(mesh, mesh.inRangeFill));
    QVERIFY(!hasMercatorLongitudeSpike(mesh, mesh.inZoneFill));

    bool reachesWestSeam = false;
    bool reachesEastSeam = false;
    bool reachesNorthLimit = false;
    const std::size_t lineEnd = mesh.inRangeLines.firstIndex + mesh.inRangeLines.indexCount;
    for (std::size_t offset = mesh.inRangeLines.firstIndex; offset < lineEnd; ++offset) {
        const std::size_t vertex = static_cast<std::size_t>(mesh.indices[offset]) * 3U;
        const double longitude =
            static_cast<double>(mesh.vertices[vertex]) + mesh.coordinateOrigin.x();
        const double mercatorY =
            static_cast<double>(mesh.vertices[vertex + 2U]) + mesh.coordinateOrigin.y();
        reachesWestSeam = reachesWestSeam || longitude <= -179.99;
        reachesEastSeam = reachesEastSeam || longitude >= 179.99;
        reachesNorthLimit = reachesNorthLimit || mercatorY >= 179.99;
    }
    QVERIFY(reachesWestSeam);
    QVERIFY(reachesEastSeam);
    QVERIFY(reachesNorthLimit);
    QVERIFY(mesh.vertices.size() / 3U <= LarZoneMeshBuilder::MaximumVertexCount);
    QVERIFY(mesh.indices.size() <= LarZoneMeshBuilder::MaximumIndexCount);
}

void LarZoneMeshTests::polarSphereCirclesUseBoundedSurfaceTessellation() {
    Target target{};
    target.ir_pos[0] = qDegreesToRadians(-79.0);
    target.ir_pos[1] = qDegreesToRadians(35.0);
    target.ir_r = 3'000'000.0;
    target.iz_pos[0] = qDegreesToRadians(-77.0);
    target.iz_pos[1] = qDegreesToRadians(15.0);
    target.iz_theta1 = 0.0;
    target.iz_theta2 = 0.0;
    target.iz_r1 = 0.0;
    target.iz_r2 = 2'600'000.0;
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Sphere);

    const LarZoneMesh mesh =
        LarZoneMeshBuilder().build(target, QBitArray(StateField::Count, true), camera, 1200, 800);

    QVERIFY(!mesh.empty());
    QVERIFY(!mesh.inputRejected);
    QVERIFY(!mesh.mercatorGeometryClipped);
    QVERIFY(mesh.inRangeFill.indexCount > 64U * 3U);
    QVERIFY(mesh.inZoneFill.indexCount > 64U * 3U);
    QVERIFY(maximumTriangleEdgeRadians(mesh, mesh.inRangeFill) < qDegreesToRadians(10.0));
    QVERIFY(maximumTriangleEdgeRadians(mesh, mesh.inZoneFill) < qDegreesToRadians(10.0));
    QVERIFY(mesh.vertices.size() / 3U <= LarZoneMeshBuilder::MaximumVertexCount);
    QVERIFY(mesh.indices.size() <= LarZoneMeshBuilder::MaximumIndexCount);
}

void LarZoneMeshTests::polarOverlayBoundsSelectWorldCopies() {
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Mercator);
    camera.setMercatorCenter(QPointF(-170.0, 0.0), 1200, 800);
    const auto copies = camera.visibleWorldCopiesForBounds(170.0, 530.0, 1200, 800);
    QCOMPARE(copies.first, -2);
    QCOMPARE(copies.last, -1);
}

void LarZoneMeshTests::hostileValuesAreRejectedWithinBudgets() {
    Target target{};
    target.ir_pos[0] = std::numeric_limits<double>::quiet_NaN();
    target.ir_pos[1] = 0.0;
    target.ir_r = std::numeric_limits<double>::infinity();
    lar::map::MapCamera camera;

    LarZoneMesh mesh = LarZoneMeshBuilder().build(
        target, available({StateField::IrPos0, StateField::IrPos1, StateField::IrR}), camera, 800,
        600);
    QVERIFY(mesh.inputRejected);
    QVERIFY(mesh.empty());
    QVERIFY(mesh.vertices.size() / 3U <= LarZoneMeshBuilder::MaximumVertexCount);
    QVERIFY(mesh.indices.size() <= LarZoneMeshBuilder::MaximumIndexCount);

    target.ir_pos[0] = 0.0;
    target.ir_r = LarZoneMeshBuilder::MaximumRadiusMeters + 1.0;
    mesh = LarZoneMeshBuilder().build(
        target, available({StateField::IrPos0, StateField::IrPos1, StateField::IrR}), camera, 800,
        600);
    QVERIFY(mesh.inputRejected);
    QVERIFY(mesh.empty());
}

void LarZoneMeshTests::extremeAnglesRemainFiniteAndBounded() {
    const double maximum = std::numeric_limits<double>::max();
    const double span = LarGeodesicGeometry::positiveAngularSpan(maximum, -maximum);
    QVERIFY(std::isfinite(span));
    QVERIFY(span > 0.0);
    QVERIFY(span <= LarGeodesicGeometry::TwoPi);

    const QPolygonF polygon = LarGeometryBuilder::inZoneWedgePolygon({10.0, -20.0}, 100.0, 200.0,
                                                                     maximum, -maximum, maximum);
    QVERIFY(!polygon.isEmpty());
    QVERIFY(polygon.size() <= 610);
    for (const QPointF &point : polygon) {
        QVERIFY(std::isfinite(point.x()));
        QVERIFY(std::isfinite(point.y()));
    }

    Target target{};
    target.iz_pos[0] = 0.25;
    target.iz_pos[1] = -0.5;
    target.iz_theta1 = maximum;
    target.iz_theta2 = -maximum;
    target.iz_r1 = 10.0;
    target.iz_r2 = 1000.0;
    const LarZoneValidationResult normalized = LarZoneInputValidator().validate(
        target, available({StateField::IzPos0, StateField::IzPos1, StateField::IzTheta1,
                           StateField::IzTheta2, StateField::IzR1, StateField::IzR2}));
    QVERIFY(!normalized.inputRejected);
    QVERIFY(normalized.inZone.has_value());
    QVERIFY(std::isfinite(normalized.inZone->startBearingRadians));
    QVERIFY(std::abs(normalized.inZone->startBearingRadians) <= LarGeodesicGeometry::Pi);

    target.iz_pos[1] = LarGeodesicGeometry::Pi + 0.01;
    const LarZoneValidationResult invalidLongitude = LarZoneInputValidator().validate(
        target, available({StateField::IzPos0, StateField::IzPos1, StateField::IzTheta1,
                           StateField::IzTheta2, StateField::IzR1, StateField::IzR2}));
    QVERIFY(invalidLongitude.inputRejected);
    QVERIFY(!invalidLongitude.inZone.has_value());
}

void LarZoneMeshTests::annularSectorProducesBoundedFillAndOutline() {
    Target target{};
    target.iz_pos[0] = 0.3;
    target.iz_pos[1] = -2.8;
    target.iz_theta1 = 5.7;
    target.iz_theta2 = 0.4;
    target.iz_r1 = 10000.0;
    target.iz_r2 = 80000.0;
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Sphere);
    camera.setSphereZoom(2.0F);

    const LarZoneMesh mesh = LarZoneMeshBuilder().build(
        target,
        available({StateField::IzPos0, StateField::IzPos1, StateField::IzTheta1,
                   StateField::IzTheta2, StateField::IzR1, StateField::IzR2}),
        camera, 1200, 800);
    QVERIFY(!mesh.empty());
    QVERIFY(!mesh.inputRejected);
    QVERIFY(mesh.inZoneFill.indexCount > 0U);
    QVERIFY(mesh.inZoneLines.indexCount > 0U);
    QVERIFY(mesh.vertices.size() / 3U <= LarZoneMeshBuilder::MaximumVertexCount);
    QVERIFY(mesh.indices.size() <= LarZoneMeshBuilder::MaximumIndexCount);
}

void LarZoneMeshTests::validatorSeparatesAvailabilityFromInvalidInput() {
    const LarZoneInputValidator validator;
    Target target{};
    LarZoneValidationResult result = validator.validate(target, {});
    QVERIFY(!result.inRange.has_value());
    QVERIFY(!result.inZone.has_value());
    QVERIFY(!result.inputRejected);

    target.ir_pos[0] = std::numeric_limits<double>::infinity();
    target.ir_pos[1] = 0.0;
    target.ir_r = 100.0;
    target.iz_pos[0] = 0.2;
    target.iz_pos[1] = -0.3;
    target.iz_theta1 = 0.1;
    target.iz_theta2 = 0.9;
    target.iz_r1 = 20.0;
    target.iz_r2 = 200.0;
    result = validator.validate(
        target, available({StateField::IrPos0, StateField::IrPos1, StateField::IrR,
                           StateField::IzPos0, StateField::IzPos1, StateField::IzTheta1,
                           StateField::IzTheta2, StateField::IzR1, StateField::IzR2}));
    QVERIFY(result.inputRejected);
    QVERIFY(!result.inRange.has_value());
    QVERIFY(result.inZone.has_value());
    QCOMPARE(result.inZone->kind, LarZoneKind::InZone);
}

void LarZoneMeshTests::samplerIsDeterministicFiniteAndBounded() {
    const LarZoneDefinition zone{LarZoneKind::InZone,
                                 {qDegreesToRadians(82.0), qDegreesToRadians(179.0)},
                                 10'000.0,
                                 1'500'000.0,
                                 0.0,
                                 LarGeodesicGeometry::TwoPi};
    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Mercator);
    camera.setMercatorZoom(5.0F, 1600, 900);
    const GeodesicZoneSampler sampler;
    const auto first = sampler.sample(zone, camera, 1600, 900);
    const auto second = sampler.sample(zone, camera, 1600, 900);

    QCOMPARE(first.radii, second.radii);
    QCOMPARE(first.bearings, second.bearings);
    QCOMPARE(first.points.size(), first.rowCount() * first.columnCount());
    QVERIFY(first.fullCircle);
    QVERIFY(first.rowCount() <=
            static_cast<std::size_t>(LarZoneMeshLimits::MaximumRadialSegmentCount + 3));
    QVERIFY(first.columnCount() <=
            static_cast<std::size_t>(LarZoneMeshLimits::MaximumAngularSegmentCount + 3));
    QCOMPARE(first.points.size(), second.points.size());
    for (std::size_t index = 0; index < first.points.size(); ++index) {
        QVERIFY(std::isfinite(first.points[index].latitude));
        QVERIFY(std::isfinite(first.points[index].longitude));
        QCOMPARE(first.points[index].latitude, second.points[index].latitude);
        QCOMPARE(first.points[index].longitude, second.points[index].longitude);
    }
}

void LarZoneMeshTests::extremeZoomUsesPixelAccurateRelativeFallback() {
    constexpr int Width = 1600;
    constexpr int Height = 900;
    Target target{};
    target.ir_pos[0] = 0.0;
    target.ir_pos[1] = qDegreesToRadians(179.0);
    target.ir_r = 10'000.0;
    const QBitArray fields = available({StateField::IrPos0, StateField::IrPos1, StateField::IrR});

    lar::map::MapCamera camera;
    camera.setPresentation(lar::map::MapPresentation::Mercator);
    camera.setMercatorCenter({179.0, 0.0}, Width, Height);
    camera.setMercatorZoom(3.0F, Width, Height);
    LarParametricZoneGpuLayer parametric;
    QVERIFY(parametric.setZones(target, fields, camera, Width, Height));

    camera.setMercatorZoom(100'000.0F, Width, Height);
    QVERIFY(!parametric.setZones(target, fields, camera, Width, Height));
    const LarZoneMesh mesh = LarZoneMeshBuilder().build(target, fields, camera, Width, Height);
    QCOMPARE(mesh.coordinateSpace, LarZoneCoordinateSpace::MercatorCameraRelative);
    QCOMPARE(mesh.coordinateOrigin, camera.mercatorCenter());
    QVERIFY(mesh.inRangeLines.indexCount > 128U * 2U);

    const std::size_t segmentCount = mesh.inRangeLines.indexCount / 2U;
    double maximumVertexError = 0.0;
    double maximumCurveError = 0.0;
    for (std::size_t segment = 0; segment < segmentCount; ++segment) {
        const std::size_t lineOffset = mesh.inRangeLines.firstIndex + segment * 2U;
        const GeoCoordinateRadians firstCoordinate = meshCoordinate(mesh, mesh.indices[lineOffset]);
        const GeoCoordinateRadians secondCoordinate =
            meshCoordinate(mesh, mesh.indices[lineOffset + 1U]);
        QPointF firstScreen;
        QPointF secondScreen;
        bool firstVisible = false;
        bool secondVisible = false;
        QVERIFY(camera.projectGeoToScreen(qRadiansToDegrees(firstCoordinate.longitude),
                                          qRadiansToDegrees(firstCoordinate.latitude), Width,
                                          Height, firstScreen, firstVisible));
        QVERIFY(camera.projectGeoToScreen(qRadiansToDegrees(secondCoordinate.longitude),
                                          qRadiansToDegrees(secondCoordinate.latitude), Width,
                                          Height, secondScreen, secondVisible));

        const double firstBearing = LarGeodesicGeometry::TwoPi * static_cast<double>(segment) /
                                    static_cast<double>(segmentCount);
        const double midpointBearing = LarGeodesicGeometry::TwoPi *
                                       (static_cast<double>(segment) + 0.5) /
                                       static_cast<double>(segmentCount);
        const GeoCoordinateRadians exactFirst = LarGeodesicGeometry::destination(
            {target.ir_pos[0], target.ir_pos[1]}, target.ir_r, firstBearing);
        const GeoCoordinateRadians exactMidpoint = LarGeodesicGeometry::destination(
            {target.ir_pos[0], target.ir_pos[1]}, target.ir_r, midpointBearing);
        QPointF exactFirstScreen;
        QPointF exactMidpointScreen;
        bool exactVisible = false;
        QVERIFY(camera.projectGeoToScreen(qRadiansToDegrees(exactFirst.longitude),
                                          qRadiansToDegrees(exactFirst.latitude), Width, Height,
                                          exactFirstScreen, exactVisible));
        QVERIFY(camera.projectGeoToScreen(qRadiansToDegrees(exactMidpoint.longitude),
                                          qRadiansToDegrees(exactMidpoint.latitude), Width, Height,
                                          exactMidpointScreen, exactVisible));
        maximumVertexError =
            std::max(maximumVertexError, std::hypot(firstScreen.x() - exactFirstScreen.x(),
                                                    firstScreen.y() - exactFirstScreen.y()));
        maximumCurveError =
            std::max(maximumCurveError,
                     pointToSegmentDistance(exactMidpointScreen, firstScreen, secondScreen));
    }
    QVERIFY2(maximumVertexError <= 0.05,
             qPrintable(QStringLiteral("relative-float vertex error was %1 px")
                            .arg(maximumVertexError, 0, 'g', 12)));
    QVERIFY2(
        maximumCurveError <= LarZoneMeshLimits::CurveErrorPixels + 0.02,
        qPrintable(
            QStringLiteral("adaptive curve error was %1 px").arg(maximumCurveError, 0, 'g', 12)));
}

QTEST_APPLESS_MAIN(LarZoneMeshTests)

#include "lar_zone_mesh_tests.moc"
