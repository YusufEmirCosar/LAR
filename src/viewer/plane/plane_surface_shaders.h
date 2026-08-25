#pragma once

/** @file plane_surface_shaders.h @brief Ground, grid, target, and flat-LAR shaders. */

namespace plane::surface::shaders {

inline constexpr char ShapeVertexLegacy[] = R"(
attribute vec3 aPosition;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec2 uOffsetXZ;
uniform float uHorizontalScale;
uniform float uVerticalScale;
uniform float uGroundHeight;
uniform float uSurfaceHalfExtent;
varying vec2 vWorldXZ;
void main() {
    vec2 worldXZ = uOffsetXZ + aPosition.xz * uHorizontalScale;
    vWorldXZ = worldXZ;
    gl_Position = uProjection * uView * vec4(worldXZ.x,
                                              uGroundHeight + aPosition.y * uVerticalScale,
                                              worldXZ.y, 1.0);
}
)";

inline constexpr char ShapeFragmentLegacy[] = R"(
uniform vec4 uColor;
uniform bool uFadeAtEdge;
uniform float uSurfaceHalfExtent;
varying vec2 vWorldXZ;
void main() {
    float normalizedDistance = length(vWorldXZ) / max(uSurfaceHalfExtent, 0.001);
    float fade = uFadeAtEdge ? 1.0 - smoothstep(0.08, 0.38, normalizedDistance) : 1.0;
    gl_FragColor = vec4(uColor.rgb, uColor.a * fade);
}
)";

inline constexpr char ShapeVertexCore[] = R"(#version 150
in vec3 aPosition;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec2 uOffsetXZ;
uniform float uHorizontalScale;
uniform float uVerticalScale;
uniform float uGroundHeight;
uniform float uSurfaceHalfExtent;
out vec2 vWorldXZ;
void main() {
    vec2 worldXZ = uOffsetXZ + aPosition.xz * uHorizontalScale;
    vWorldXZ = worldXZ;
    gl_Position = uProjection * uView * vec4(worldXZ.x,
                                              uGroundHeight + aPosition.y * uVerticalScale,
                                              worldXZ.y, 1.0);
}
)";

inline constexpr char ShapeFragmentCore[] = R"(#version 150
uniform vec4 uColor;
uniform bool uFadeAtEdge;
uniform float uSurfaceHalfExtent;
in vec2 vWorldXZ;
out vec4 fragmentColor;
void main() {
    float normalizedDistance = length(vWorldXZ) / max(uSurfaceHalfExtent, 0.001);
    float fade = uFadeAtEdge ? 1.0 - smoothstep(0.08, 0.38, normalizedDistance) : 1.0;
    fragmentColor = vec4(uColor.rgb, uColor.a * fade);
}
)";

inline constexpr char ZoneVertexLegacy[] = R"(
attribute vec2 aParam;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec2 uCenterXZ;
uniform vec2 uRadii;
uniform vec2 uStartSpan;
uniform float uGroundHeight;
void main() {
    float radius = mix(uRadii.x, uRadii.y, aParam.x);
    float bearing = uStartSpan.x + uStartSpan.y * aParam.y;
    vec2 direction = vec2(sin(bearing), -cos(bearing));
    vec2 worldXZ = uCenterXZ + direction * radius;
    gl_Position = uProjection * uView * vec4(worldXZ.x, uGroundHeight, worldXZ.y, 1.0);
}
)";

inline constexpr char ZoneFragmentLegacy[] = R"(
uniform vec4 uColor;
void main() { gl_FragColor = uColor; }
)";

inline constexpr char ZoneVertexCore[] = R"(#version 150
in vec2 aParam;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec2 uCenterXZ;
uniform vec2 uRadii;
uniform vec2 uStartSpan;
uniform float uGroundHeight;
void main() {
    float radius = mix(uRadii.x, uRadii.y, aParam.x);
    float bearing = uStartSpan.x + uStartSpan.y * aParam.y;
    vec2 direction = vec2(sin(bearing), -cos(bearing));
    vec2 worldXZ = uCenterXZ + direction * radius;
    gl_Position = uProjection * uView * vec4(worldXZ.x, uGroundHeight, worldXZ.y, 1.0);
}
)";

inline constexpr char ZoneFragmentCore[] = R"(#version 150
uniform vec4 uColor;
out vec4 fragmentColor;
void main() { fragmentColor = uColor; }
)";

} // namespace plane::surface::shaders
