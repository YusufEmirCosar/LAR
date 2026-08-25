#pragma once

/** @file plane_shaders.h @brief Embedded legacy/core shaders for the Plane workspace. */

namespace plane::shaders {

inline constexpr char MeshVertexLegacy[] = R"(
attribute vec3 aPosition;
attribute vec3 aNormal;
attribute vec2 aTexCoord;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
uniform float uPointSize;
varying vec3 vNormal;
varying vec2 vTexCoord;
void main() {
    vNormal = normalize(uNormalMatrix * aNormal);
    vTexCoord = aTexCoord;
    gl_PointSize = uPointSize;
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
)";

inline constexpr char MeshFragmentLegacy[] = R"(
uniform vec4 uColor;
uniform vec3 uLightDirection;
uniform bool uUnlit;
uniform bool uHasBaseColorTexture;
uniform sampler2D uBaseColorTexture;
varying vec3 vNormal;
varying vec2 vTexCoord;
void main() {
    float lighting = uUnlit ? 1.0 : 0.68 + 0.32 * max(dot(normalize(vNormal), normalize(uLightDirection)), 0.0);
    vec4 baseColor = uColor;
    if (uHasBaseColorTexture) baseColor *= texture2D(uBaseColorTexture, vTexCoord);
    gl_FragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
)";

inline constexpr char MeshVertexCore[] = R"(#version 150
in vec3 aPosition;
in vec3 aNormal;
in vec2 aTexCoord;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
uniform float uPointSize;
out vec3 vNormal;
out vec2 vTexCoord;
void main() {
    vNormal = normalize(uNormalMatrix * aNormal);
    vTexCoord = aTexCoord;
    gl_PointSize = uPointSize;
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
)";

inline constexpr char MeshFragmentCore[] = R"(#version 150
uniform vec4 uColor;
uniform vec3 uLightDirection;
uniform bool uUnlit;
uniform bool uHasBaseColorTexture;
uniform sampler2D uBaseColorTexture;
in vec3 vNormal;
in vec2 vTexCoord;
out vec4 fragmentColor;
void main() {
    float lighting = uUnlit ? 1.0 : 0.68 + 0.32 * max(dot(normalize(vNormal), normalize(uLightDirection)), 0.0);
    vec4 baseColor = uColor;
    if (uHasBaseColorTexture) baseColor *= texture(uBaseColorTexture, vTexCoord);
    fragmentColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
)";

inline constexpr char SkyVertexLegacy[] = R"(
attribute vec3 aPosition;
uniform mat3 uViewRotation;
uniform mat4 uProjection;
varying vec3 vDirection;
void main() {
    vDirection = aPosition;
    vec4 projected = uProjection * vec4(uViewRotation * aPosition, 1.0);
    gl_Position = projected.xyww;
}
)";

inline constexpr char SkyFragmentLegacy[] = R"(
uniform samplerCube uSkybox;
varying vec3 vDirection;
void main() { gl_FragColor = textureCube(uSkybox, vDirection); }
)";

inline constexpr char SkyVertexCore[] = R"(#version 150
in vec3 aPosition;
uniform mat3 uViewRotation;
uniform mat4 uProjection;
out vec3 vDirection;
void main() {
    vDirection = aPosition;
    vec4 projected = uProjection * vec4(uViewRotation * aPosition, 1.0);
    gl_Position = projected.xyww;
}
)";

inline constexpr char SkyFragmentCore[] = R"(#version 150
uniform samplerCube uSkybox;
in vec3 vDirection;
out vec4 fragmentColor;
void main() { fragmentColor = texture(uSkybox, vDirection); }
)";

} // namespace plane::shaders
