#pragma once

/**
 * @file map_shaders.h
 * @brief Embedded legacy and core-profile GLSL map shader sources.
 */

namespace lar::map::shaders {

inline constexpr char VertexLegacy[] = R"(
attribute vec3 aPosition;
uniform bool uSphere;
uniform bool uScreenSpace;
uniform float uWorldOffset;
uniform vec2 uSphereCenterHigh;
uniform vec2 uSphereCenterLow;
uniform vec2 uSphereLatitudeSinCos;
uniform float uBearing;
uniform vec2 uMercatorCenter;
uniform mat4 uProjection;
varying float vDepth;

vec2 rotateForBearing(vec2 value) {
    float cosine = cos(uBearing);
    float sine = sin(uBearing);
    return vec2(
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine);
}

vec3 projectSphere(vec2 coordinateDegrees) {
    float degreesToRadians = 3.14159265358979323846 / 180.0;
    vec2 delta = ((coordinateDegrees - uSphereCenterHigh) - uSphereCenterLow)
                 * degreesToRadians;
    float sinLongitude = sin(delta.x);
    float cosLongitude = cos(delta.x);
    float sinLatitude = sin(delta.y);
    float cosLatitude = cos(delta.y);
    float centerSin = uSphereLatitudeSinCos.x;
    float centerCos = uSphereLatitudeSinCos.y;
    float oneMinusCosLongitude = 1.0 - cosLongitude;
    float x = (centerCos * cosLatitude - centerSin * sinLatitude) * sinLongitude;
    float y = centerSin * centerCos * cosLatitude * oneMinusCosLongitude
              + (centerCos * centerCos + centerSin * centerSin * cosLongitude) * sinLatitude;
    float z = (centerSin * centerSin + centerCos * centerCos * cosLongitude) * cosLatitude
              + centerSin * centerCos * oneMinusCosLongitude * sinLatitude;
    return vec3(x, y, z);
}

void main() {
    if (uScreenSpace) {
        vDepth = 1.0;
        gl_Position = uProjection * vec4(aPosition.xy, 0.0, 1.0);
    } else if (!uSphere) {
        vDepth = 1.0;
        vec2 mapPosition = vec2(aPosition.x + uWorldOffset, aPosition.z);
        vec2 viewPosition = uMercatorCenter + rotateForBearing(mapPosition - uMercatorCenter);
        gl_Position = uProjection * vec4(viewPosition, 0.0, 1.0);
    } else {
        vec3 spherePosition = projectSphere(aPosition.xy);
        vDepth = spherePosition.z;
        gl_Position = uProjection * vec4(rotateForBearing(spherePosition.xy), 0.0, 1.0);
    }
}
)";

inline constexpr char FragmentLegacy[] = R"(
uniform vec4 uColor;
uniform bool uKeepBack;
varying float vDepth;

void main() {
    if (!uKeepBack && vDepth < 0.0) {
        discard;
    }
    gl_FragColor = uColor;
}
)";

inline constexpr char VertexCore[] = R"(#version 150
in vec3 aPosition;
uniform bool uSphere;
uniform bool uScreenSpace;
uniform float uWorldOffset;
uniform vec2 uSphereCenterHigh;
uniform vec2 uSphereCenterLow;
uniform vec2 uSphereLatitudeSinCos;
uniform float uBearing;
uniform vec2 uMercatorCenter;
uniform mat4 uProjection;
out float vDepth;

vec2 rotateForBearing(vec2 value) {
    float cosine = cos(uBearing);
    float sine = sin(uBearing);
    return vec2(
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine);
}

vec3 projectSphere(vec2 coordinateDegrees) {
    float degreesToRadians = 3.14159265358979323846 / 180.0;
    vec2 delta = ((coordinateDegrees - uSphereCenterHigh) - uSphereCenterLow)
                 * degreesToRadians;
    float sinLongitude = sin(delta.x);
    float cosLongitude = cos(delta.x);
    float sinLatitude = sin(delta.y);
    float cosLatitude = cos(delta.y);
    float centerSin = uSphereLatitudeSinCos.x;
    float centerCos = uSphereLatitudeSinCos.y;
    float oneMinusCosLongitude = 1.0 - cosLongitude;
    float x = (centerCos * cosLatitude - centerSin * sinLatitude) * sinLongitude;
    float y = centerSin * centerCos * cosLatitude * oneMinusCosLongitude
              + (centerCos * centerCos + centerSin * centerSin * cosLongitude) * sinLatitude;
    float z = (centerSin * centerSin + centerCos * centerCos * cosLongitude) * cosLatitude
              + centerSin * centerCos * oneMinusCosLongitude * sinLatitude;
    return vec3(x, y, z);
}

void main() {
    if (uScreenSpace) {
        vDepth = 1.0;
        gl_Position = uProjection * vec4(aPosition.xy, 0.0, 1.0);
    } else if (!uSphere) {
        vDepth = 1.0;
        vec2 mapPosition = vec2(aPosition.x + uWorldOffset, aPosition.z);
        vec2 viewPosition = uMercatorCenter + rotateForBearing(mapPosition - uMercatorCenter);
        gl_Position = uProjection * vec4(viewPosition, 0.0, 1.0);
    } else {
        vec3 spherePosition = projectSphere(aPosition.xy);
        vDepth = spherePosition.z;
        gl_Position = uProjection * vec4(rotateForBearing(spherePosition.xy), 0.0, 1.0);
    }
}
)";

inline constexpr char FragmentCore[] = R"(#version 150
uniform vec4 uColor;
uniform bool uKeepBack;
in float vDepth;
out vec4 fragmentColor;

void main() {
    if (!uKeepBack && vDepth < 0.0) {
        discard;
    }
    fragmentColor = uColor;
}
)";

} // namespace lar::map::shaders
