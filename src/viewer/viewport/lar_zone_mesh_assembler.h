#pragma once

/**
 * @file lar_zone_mesh_assembler.h
 * @brief Bounded conversion from geodesic samples to indexed GPU geometry.
 */

#include "viewer/map/map_camera.h"
#include "viewer/viewport/geodesic_zone_sampler.h"
#include "viewer/viewport/lar_zone_mesh.h"

/** Converts a geographic sample grid into bounded GPU vertex/index ranges. */
class LarZoneMeshAssembler final {
  public:
    [[nodiscard]] bool append(const LarZoneDefinition &zone, const GeodesicZoneSampleGrid &samples,
                              LarZoneMesh &mesh, LarZoneDrawRange &fillRange,
                              LarZoneDrawRange &lineRange) const;
};
