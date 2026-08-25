
#include "viewer/viewport/lar_zone_mesh_builder.h"

#include "viewer/viewport/geodesic_zone_sampler.h"
#include "viewer/viewport/lar_zone_input_validator.h"
#include "viewer/viewport/lar_zone_mesh_assembler.h"

LarZoneMesh LarZoneMeshBuilder::build(const Target &target, const QBitArray &availableFields,
                                      const lar::map::MapCamera &camera, int viewportWidth,
                                      int viewportHeight) const {
    const LarZoneValidationResult validated =
        LarZoneInputValidator().validate(target, availableFields);
    LarZoneMesh mesh;
    if (camera.presentation() == lar::map::MapPresentation::Mercator) {
        mesh.coordinateSpace = LarZoneCoordinateSpace::MercatorCameraRelative;
        mesh.coordinateOrigin = camera.mercatorCenter();
    } else {
        mesh.coordinateSpace = LarZoneCoordinateSpace::SphereCameraRelative;
        mesh.coordinateOrigin = camera.sphereCenter();
    }
    mesh.vertices.reserve(4096U * 3U);
    mesh.indices.reserve(16384U);
    const GeodesicZoneSampler sampler;
    const LarZoneMeshAssembler assembler;
    const auto append = [&](const LarZoneDefinition &zone, LarZoneDrawRange &fill,
                            LarZoneDrawRange &lines) {
        return assembler.append(zone, sampler.sample(zone, camera, viewportWidth, viewportHeight),
                                mesh, fill, lines);
    };
    bool assembled = true;
    if (validated.inRange) {
        assembled = append(*validated.inRange, mesh.inRangeFill, mesh.inRangeLines);
    }
    if (assembled && validated.inZone) {
        assembled = append(*validated.inZone, mesh.inZoneFill, mesh.inZoneLines);
    }
    if (!assembled || mesh.vertices.size() / 3U > MaximumVertexCount ||
        mesh.indices.size() > MaximumIndexCount) {
        mesh.clear();
        mesh.inputRejected = true;
        return mesh;
    }
    mesh.inputRejected = validated.inputRejected;
    return mesh;
}
