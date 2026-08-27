#pragma once

/** @file plane_terrain_shaders.h @brief Lit hypsometric and bathymetric DTED shaders. */

namespace plane::terrain::shaders {

inline constexpr char VertexLegacy[] = R"(
attribute vec3 aPosition;
attribute vec3 aNormal;
attribute float aWaterDepth;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec2 uOffsetXZ;
uniform vec2 uHorizontalScale;
uniform float uAircraftAltitudeScene;
uniform float uMetersPerSceneUnit;
uniform float uPatchHalfExtentScene;
uniform float uRenderWater;
varying vec3 vNormal;
varying float vElevationMeters;
varying float vWaterDepthMeters;
varying vec2 vLandMaskUv;
void main() {
    float waterPass = step(0.5, uRenderWater);
    vec3 worldPosition = vec3(aPosition.x * uHorizontalScale.x + uOffsetXZ.x,
                              mix(aPosition.y, 0.0, waterPass) - uAircraftAltitudeScene,
                              aPosition.z * uHorizontalScale.y + uOffsetXZ.y);
    vNormal = mix(aNormal, vec3(0.0, 1.0, 0.0), waterPass);
    vElevationMeters = aPosition.y * uMetersPerSceneUnit;
    vWaterDepthMeters = aWaterDepth;
    vLandMaskUv = vec2(aPosition.x, -aPosition.z) / (2.0 * uPatchHalfExtentScene) + 0.5;
    gl_Position = uProjection * uView * vec4(worldPosition, 1.0);
}
)";

inline constexpr char FragmentLegacy[] = R"(
uniform float uMinimumElevationMeters;
uniform float uMaximumElevationMeters;
uniform sampler2D uLandMask;
uniform float uUseLandMask;
uniform float uUniformLand;
uniform float uRenderWater;
varying vec3 vNormal;
varying float vElevationMeters;
varying float vWaterDepthMeters;
varying vec2 vLandMaskUv;
void main() {
    float land = uUniformLand;
    if (uUseLandMask > 0.5) {
        land = step(0.5, texture2D(uLandMask, vLandMaskUv).r);
    }
    float selected = uRenderWater > 0.5 ? 1.0 - land : land;
    if (selected < 0.5) {
        discard;
    }
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
    vec3 lightDirection = normalize(vec3(0.36, 0.86, 0.38));
    float diffuse = max(dot(normalize(vNormal), lightDirection), 0.0);
    vec3 litTerrain = terrainColor * (0.46 + 0.54 * diffuse);
    vec3 litWater = waterColor * (0.76 + 0.24 * diffuse);
    gl_FragColor = vec4(uRenderWater > 0.5 ? litWater : litTerrain, 1.0);
}
)";

inline constexpr char VertexCore[] = R"(#version 150
in vec3 aPosition;
in vec3 aNormal;
in float aWaterDepth;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec2 uOffsetXZ;
uniform vec2 uHorizontalScale;
uniform float uAircraftAltitudeScene;
uniform float uMetersPerSceneUnit;
uniform float uPatchHalfExtentScene;
uniform float uRenderWater;
out vec3 vNormal;
out float vElevationMeters;
out float vWaterDepthMeters;
out vec2 vLandMaskUv;
void main() {
    float waterPass = step(0.5, uRenderWater);
    vec3 worldPosition = vec3(aPosition.x * uHorizontalScale.x + uOffsetXZ.x,
                              mix(aPosition.y, 0.0, waterPass) - uAircraftAltitudeScene,
                              aPosition.z * uHorizontalScale.y + uOffsetXZ.y);
    vNormal = mix(aNormal, vec3(0.0, 1.0, 0.0), waterPass);
    vElevationMeters = aPosition.y * uMetersPerSceneUnit;
    vWaterDepthMeters = aWaterDepth;
    vLandMaskUv = vec2(aPosition.x, -aPosition.z) / (2.0 * uPatchHalfExtentScene) + 0.5;
    gl_Position = uProjection * uView * vec4(worldPosition, 1.0);
}
)";

inline constexpr char FragmentCore[] = R"(#version 150
uniform float uMinimumElevationMeters;
uniform float uMaximumElevationMeters;
uniform sampler2D uLandMask;
uniform float uUseLandMask;
uniform float uUniformLand;
uniform float uRenderWater;
in vec3 vNormal;
in float vElevationMeters;
in float vWaterDepthMeters;
in vec2 vLandMaskUv;
out vec4 fragmentColor;
void main() {
    float land = uUniformLand;
    if (uUseLandMask > 0.5) {
        land = step(0.5, texture(uLandMask, vLandMaskUv).r);
    }
    float selected = uRenderWater > 0.5 ? 1.0 - land : land;
    if (selected < 0.5) {
        discard;
    }
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
    vec3 lightDirection = normalize(vec3(0.36, 0.86, 0.38));
    float diffuse = max(dot(normalize(vNormal), lightDirection), 0.0);
    vec3 litTerrain = terrainColor * (0.46 + 0.54 * diffuse);
    vec3 litWater = waterColor * (0.76 + 0.24 * diffuse);
    fragmentColor = vec4(uRenderWater > 0.5 ? litWater : litTerrain, 1.0);
}
)";

} // namespace plane::terrain::shaders
