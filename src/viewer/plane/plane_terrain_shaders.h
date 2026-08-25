#pragma once

/** @file plane_terrain_shaders.h @brief Lit hypsometric and bathymetric DTED shaders. */

namespace plane::terrain::shaders {

inline constexpr char VertexLegacy[] = R"(
attribute vec3 aPosition;
attribute vec3 aNormal;
attribute vec2 aWater;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec2 uOffsetXZ;
uniform float uAircraftAltitudeScene;
uniform float uMetersPerSceneUnit;
varying vec3 vNormal;
varying float vElevationMeters;
varying float vWaterMask;
varying float vWaterDepthMeters;
void main() {
    vec3 worldPosition = vec3(aPosition.x + uOffsetXZ.x,
                              aPosition.y - uAircraftAltitudeScene,
                              aPosition.z + uOffsetXZ.y);
    vNormal = aNormal;
    vElevationMeters = aPosition.y * uMetersPerSceneUnit;
    vWaterMask = aWater.x;
    vWaterDepthMeters = aWater.y;
    gl_Position = uProjection * uView * vec4(worldPosition, 1.0);
}
)";

inline constexpr char FragmentLegacy[] = R"(
uniform float uMinimumElevationMeters;
uniform float uMaximumElevationMeters;
varying vec3 vNormal;
varying float vElevationMeters;
varying float vWaterMask;
varying float vWaterDepthMeters;
void main() {
    float rangeMeters = max(1.0, uMaximumElevationMeters - uMinimumElevationMeters);
    float height = clamp((vElevationMeters - uMinimumElevationMeters) / rangeMeters, 0.0, 1.0);
    vec3 lowColor = vec3(0.16, 0.31, 0.22);
    vec3 middleColor = vec3(0.43, 0.42, 0.25);
    vec3 highColor = vec3(0.72, 0.70, 0.63);
    vec3 terrainColor = mix(lowColor, middleColor, smoothstep(0.0, 0.58, height));
    terrainColor = mix(terrainColor, highColor, smoothstep(0.58, 1.0, height));
    float depth = clamp(log(1.0 + max(0.0, vWaterDepthMeters)) / log(11001.0), 0.0, 1.0);
    vec3 shallowWater = vec3(0.08, 0.48, 0.76);
    vec3 middleWater = vec3(0.025, 0.20, 0.48);
    vec3 deepWater = vec3(0.004, 0.025, 0.12);
    vec3 waterColor = mix(shallowWater, middleWater, smoothstep(0.0, 0.62, depth));
    waterColor = mix(waterColor, deepWater, smoothstep(0.62, 1.0, depth));
    float water = step(0.5, vWaterMask);
    vec3 lightDirection = normalize(vec3(0.36, 0.86, 0.38));
    float diffuse = max(dot(normalize(vNormal), lightDirection), 0.0);
    vec3 litTerrain = terrainColor * (0.46 + 0.54 * diffuse);
    vec3 litWater = waterColor * (0.76 + 0.24 * diffuse);
    gl_FragColor = vec4(mix(litTerrain, litWater, water), 1.0);
}
)";

inline constexpr char VertexCore[] = R"(#version 150
in vec3 aPosition;
in vec3 aNormal;
in vec2 aWater;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec2 uOffsetXZ;
uniform float uAircraftAltitudeScene;
uniform float uMetersPerSceneUnit;
out vec3 vNormal;
out float vElevationMeters;
out float vWaterMask;
out float vWaterDepthMeters;
void main() {
    vec3 worldPosition = vec3(aPosition.x + uOffsetXZ.x,
                              aPosition.y - uAircraftAltitudeScene,
                              aPosition.z + uOffsetXZ.y);
    vNormal = aNormal;
    vElevationMeters = aPosition.y * uMetersPerSceneUnit;
    vWaterMask = aWater.x;
    vWaterDepthMeters = aWater.y;
    gl_Position = uProjection * uView * vec4(worldPosition, 1.0);
}
)";

inline constexpr char FragmentCore[] = R"(#version 150
uniform float uMinimumElevationMeters;
uniform float uMaximumElevationMeters;
in vec3 vNormal;
in float vElevationMeters;
in float vWaterMask;
in float vWaterDepthMeters;
out vec4 fragmentColor;
void main() {
    float rangeMeters = max(1.0, uMaximumElevationMeters - uMinimumElevationMeters);
    float height = clamp((vElevationMeters - uMinimumElevationMeters) / rangeMeters, 0.0, 1.0);
    vec3 lowColor = vec3(0.16, 0.31, 0.22);
    vec3 middleColor = vec3(0.43, 0.42, 0.25);
    vec3 highColor = vec3(0.72, 0.70, 0.63);
    vec3 terrainColor = mix(lowColor, middleColor, smoothstep(0.0, 0.58, height));
    terrainColor = mix(terrainColor, highColor, smoothstep(0.58, 1.0, height));
    float depth = clamp(log(1.0 + max(0.0, vWaterDepthMeters)) / log(11001.0), 0.0, 1.0);
    vec3 shallowWater = vec3(0.08, 0.48, 0.76);
    vec3 middleWater = vec3(0.025, 0.20, 0.48);
    vec3 deepWater = vec3(0.004, 0.025, 0.12);
    vec3 waterColor = mix(shallowWater, middleWater, smoothstep(0.0, 0.62, depth));
    waterColor = mix(waterColor, deepWater, smoothstep(0.62, 1.0, depth));
    float water = step(0.5, vWaterMask);
    vec3 lightDirection = normalize(vec3(0.36, 0.86, 0.38));
    float diffuse = max(dot(normalize(vNormal), lightDirection), 0.0);
    vec3 litTerrain = terrainColor * (0.46 + 0.54 * diffuse);
    vec3 litWater = waterColor * (0.76 + 0.24 * diffuse);
    fragmentColor = vec4(mix(litTerrain, litWater, water), 1.0);
}
)";

} // namespace plane::terrain::shaders
