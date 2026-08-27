#include "viewer/plane/glb_model_reader.h"
#include "viewer/plane/glb_resource_limits.h"
#include "viewer/plane/glb_resource_reader.h"
#include "viewer/plane/glb_texture_reader.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace {
constexpr std::size_t MaximumVertices = 250000U;
constexpr std::size_t MaximumIndices = 1500000U;

GlbModelReadResult failure(const QString &message) {
    return {nullptr, message};
}

quint32 littleU32(const char *source) noexcept {
    quint32 encoded = 0U;
    std::memcpy(&encoded, source, sizeof(encoded));
    return qFromLittleEndian(encoded);
}

quint16 littleU16(const char *source) noexcept {
    quint16 encoded = 0U;
    std::memcpy(&encoded, source, sizeof(encoded));
    return qFromLittleEndian(encoded);
}

float littleFloat(const char *source) noexcept {
    const quint32 decoded = littleU32(source);
    float value = 0.0F;
    std::memcpy(&value, &decoded, sizeof(value));
    return value;
}

struct AccessorSpan final {
    const char *data = nullptr;
    int count = 0;
    int stride = 0;
    int componentType = 0;
    QString type;
};

struct ReaderContext final {
    QJsonArray accessors;
    QJsonArray bufferViews;
    QJsonArray materials;
    QJsonArray meshes;
    QJsonArray nodes;
    QJsonArray images;
    QJsonArray textures;
    QJsonArray samplers;
    std::vector<QByteArray> binaries;
    QString sourceDirectory;
};

bool integerValue(const QJsonObject &object, const QString &key, int *value, int fallback = -1) {
    const QJsonValue candidate = object.value(key);
    if (candidate.isUndefined()) {
        *value = fallback;
        return fallback >= 0;
    }
    if (!candidate.isDouble()) {
        return false;
    }
    const qint64 integer = candidate.toInteger(std::numeric_limits<qint64>::min());
    if (integer < 0 || integer > std::numeric_limits<int>::max()) {
        return false;
    }
    *value = static_cast<int>(integer);
    return true;
}

bool accessorSpan(const ReaderContext &context, int accessorIndex, int elementBytes,
                  AccessorSpan *result) {
    if (result == nullptr || accessorIndex < 0 || accessorIndex >= context.accessors.size()) {
        return false;
    }
    const QJsonObject accessor = context.accessors[accessorIndex].toObject();
    if (!accessor.value(QStringLiteral("sparse")).isUndefined()) {
        return false;
    }
    int viewIndex = -1;
    int count = -1;
    int componentType = -1;
    if (!integerValue(accessor, QStringLiteral("bufferView"), &viewIndex) ||
        !integerValue(accessor, QStringLiteral("count"), &count) ||
        !integerValue(accessor, QStringLiteral("componentType"), &componentType) || count <= 0 ||
        viewIndex >= context.bufferViews.size() ||
        !accessor.value(QStringLiteral("type")).isString()) {
        return false;
    }
    const QJsonObject view = context.bufferViews[viewIndex].toObject();
    int bufferIndex = -1;
    int viewLength = -1;
    if (!integerValue(view, QStringLiteral("buffer"), &bufferIndex) || bufferIndex < 0 ||
        bufferIndex >= static_cast<int>(context.binaries.size()) ||
        !integerValue(view, QStringLiteral("byteLength"), &viewLength) || viewLength <= 0) {
        return false;
    }
    const QByteArray &binary = context.binaries[static_cast<std::size_t>(bufferIndex)];
    const int viewOffset = view.value(QStringLiteral("byteOffset")).toInt(0);
    const int accessorOffset = accessor.value(QStringLiteral("byteOffset")).toInt(0);
    const int stride = view.value(QStringLiteral("byteStride")).toInt(elementBytes);
    if (viewOffset < 0 || accessorOffset < 0 || stride < elementBytes || stride > 256) {
        return false;
    }
    const qint64 first = static_cast<qint64>(viewOffset) + accessorOffset;
    const qint64 required = static_cast<qint64>(count - 1) * stride + elementBytes;
    const qint64 viewEnd = static_cast<qint64>(viewOffset) + viewLength;
    // Validate the complete strided range against both its declared bufferView
    // and the resolved buffer. Checking only the first element would let a
    // crafted count/stride escape the view late in iteration.
    if (first < viewOffset || required <= 0 || first + required > viewEnd ||
        first + required > binary.size()) {
        return false;
    }
    *result = {binary.constData() + first, count, stride, componentType,
               accessor.value(QStringLiteral("type")).toString()};
    return true;
}

int componentByteSize(int componentType) noexcept {
    switch (componentType) {
    case 5121:
        return 1;
    case 5123:
        return 2;
    case 5125:
    case 5126:
        return 4;
    default:
        return 0;
    }
}

bool optionalIndex(const QJsonObject &object, const QString &key, int *value) {
    if (object.value(key).isUndefined()) {
        *value = -1;
        return true;
    }
    return integerValue(object, key, value);
}

QMatrix4x4 nodeTransform(const QJsonObject &node, bool *valid) {
    *valid = true;
    QMatrix4x4 transform;
    const QJsonValue matrixValue = node.value(QStringLiteral("matrix"));
    if (matrixValue.isArray()) {
        const QJsonArray values = matrixValue.toArray();
        if (values.size() != 16) {
            *valid = false;
            return {};
        }
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                const double value =
                    values[column * 4 + row].toDouble(std::numeric_limits<double>::quiet_NaN());
                if (!std::isfinite(value)) {
                    *valid = false;
                    return {};
                }
                transform(row, column) = static_cast<float>(value);
            }
        }
        return transform;
    }

    const auto vector = [valid](const QJsonValue &candidate, int expected,
                                const std::vector<float> &fallback) {
        if (candidate.isUndefined()) {
            return fallback;
        }
        const QJsonArray values = candidate.toArray();
        if (values.size() != expected) {
            *valid = false;
            return fallback;
        }
        std::vector<float> result;
        result.reserve(static_cast<std::size_t>(expected));
        for (const QJsonValue &entry : values) {
            const double value = entry.toDouble(std::numeric_limits<double>::quiet_NaN());
            if (!std::isfinite(value)) {
                *valid = false;
                return fallback;
            }
            result.push_back(static_cast<float>(value));
        }
        return result;
    };
    const auto translation =
        vector(node.value(QStringLiteral("translation")), 3, {0.0F, 0.0F, 0.0F});
    const auto rotation =
        vector(node.value(QStringLiteral("rotation")), 4, {0.0F, 0.0F, 0.0F, 1.0F});
    const auto scale = vector(node.value(QStringLiteral("scale")), 3, {1.0F, 1.0F, 1.0F});
    if (!*valid) {
        return {};
    }
    transform.translate(translation[0], translation[1], translation[2]);
    transform.rotate(QQuaternion(rotation[3], rotation[0], rotation[1], rotation[2]).normalized());
    transform.scale(scale[0], scale[1], scale[2]);
    return transform;
}

struct MaterialInfo final {
    QVector4D baseColor{0.35F, 0.38F, 0.4F, 1.0F};
    int baseColorTexture = -1;
    bool doubleSided = false;
};

bool materialInfo(const ReaderContext &context, int materialIndex, MaterialInfo *result) {
    if (result == nullptr) {
        return false;
    }
    *result = {};
    if (materialIndex < 0 || materialIndex >= context.materials.size()) {
        return materialIndex < 0;
    }
    const QJsonObject material = context.materials[materialIndex].toObject();
    result->doubleSided = material.value(QStringLiteral("doubleSided")).toBool(false);
    const QJsonObject pbr = material.value(QStringLiteral("pbrMetallicRoughness")).toObject();
    const QJsonValue factorValue = pbr.value(QStringLiteral("baseColorFactor"));
    result->baseColor = QVector4D(1.0F, 1.0F, 1.0F, 1.0F);
    if (!factorValue.isUndefined()) {
        const QJsonArray factor = factorValue.toArray();
        if (factor.size() != 4) {
            return false;
        }
        for (int index = 0; index < 4; ++index) {
            const double component = factor[index].toDouble(-1.0);
            if (!std::isfinite(component) || component < 0.0 || component > 1.0) {
                return false;
            }
            result->baseColor[index] = static_cast<float>(component);
        }
    }

    const QJsonValue textureValue = pbr.value(QStringLiteral("baseColorTexture"));
    if (!textureValue.isUndefined()) {
        const QJsonObject texture = textureValue.toObject();
        int textureIndex = -1;
        int textureCoordinates = 0;
        if (!textureValue.isObject() ||
            !integerValue(texture, QStringLiteral("index"), &textureIndex) ||
            textureIndex >= context.textures.size() ||
            !integerValue(texture, QStringLiteral("texCoord"), &textureCoordinates, 0) ||
            textureCoordinates != 0) {
            return false;
        }
        result->baseColorTexture = textureIndex;
    }
    return true;
}

bool appendPrimitive(const ReaderContext &context, const QJsonObject &primitive,
                     const QMatrix4x4 &transform, PlaneModelMesh *mesh) {
    if (primitive.value(QStringLiteral("mode")).toInt(4) != 4) {
        return false;
    }
    const QJsonObject attributes = primitive.value(QStringLiteral("attributes")).toObject();
    int positionIndex = -1;
    int normalIndex = -1;
    int textureCoordinateIndex = -1;
    int indicesIndex = -1;
    int materialIndex = -1;
    if (!integerValue(attributes, QStringLiteral("POSITION"), &positionIndex) ||
        !integerValue(attributes, QStringLiteral("NORMAL"), &normalIndex) ||
        !optionalIndex(attributes, QStringLiteral("TEXCOORD_0"), &textureCoordinateIndex) ||
        !optionalIndex(primitive, QStringLiteral("indices"), &indicesIndex) ||
        !optionalIndex(primitive, QStringLiteral("material"), &materialIndex)) {
        return false;
    }
    MaterialInfo material;
    if (!materialInfo(context, materialIndex, &material) ||
        (material.baseColorTexture >= 0 && textureCoordinateIndex < 0)) {
        return false;
    }
    AccessorSpan positions;
    AccessorSpan normals;
    AccessorSpan textureCoordinates;
    AccessorSpan indices;
    int indexComponentType = 0;
    if (indicesIndex >= 0) {
        if (indicesIndex >= context.accessors.size() ||
            !integerValue(context.accessors[indicesIndex].toObject(),
                          QStringLiteral("componentType"), &indexComponentType) ||
            componentByteSize(indexComponentType) == 0) {
            return false;
        }
    }
    if (!accessorSpan(context, positionIndex, 12, &positions) ||
        !accessorSpan(context, normalIndex, 12, &normals) || positions.count != normals.count ||
        positions.componentType != 5126 || normals.componentType != 5126 ||
        positions.type != QStringLiteral("VEC3") || normals.type != QStringLiteral("VEC3")) {
        return false;
    }
    if (indicesIndex >= 0 &&
        (!accessorSpan(context, indicesIndex, componentByteSize(indexComponentType), &indices) ||
         indices.componentType != indexComponentType || indices.type != QStringLiteral("SCALAR") ||
         indices.count % 3 != 0)) {
        return false;
    }
    if (indicesIndex < 0 && positions.count % 3 != 0) {
        return false;
    }
    if (textureCoordinateIndex >= 0 &&
        (!accessorSpan(context, textureCoordinateIndex, 8, &textureCoordinates) ||
         textureCoordinates.count != positions.count || textureCoordinates.componentType != 5126 ||
         textureCoordinates.type != QStringLiteral("VEC2"))) {
        return false;
    }
    const std::size_t baseVertex = mesh->vertexCount();
    const int indexCount = indicesIndex >= 0 ? indices.count : positions.count;
    if (baseVertex + static_cast<std::size_t>(positions.count) > MaximumVertices ||
        mesh->indices.size() + static_cast<std::size_t>(indexCount) > MaximumIndices) {
        return false;
    }
    const QMatrix3x3 normalTransform = transform.normalMatrix();
    mesh->vertices.reserve(mesh->vertices.size() + static_cast<std::size_t>(positions.count) *
                                                       PlaneModelVertexStrideFloats);
    for (int index = 0; index < positions.count; ++index) {
        const char *positionData = positions.data + static_cast<qint64>(index) * positions.stride;
        const char *normalData = normals.data + static_cast<qint64>(index) * normals.stride;
        const QVector3D position(littleFloat(positionData), littleFloat(positionData + 4),
                                 littleFloat(positionData + 8));
        QVector3D normal(littleFloat(normalData), littleFloat(normalData + 4),
                         littleFloat(normalData + 8));
        const QVector3D transformedPosition = transform.map(position);
        normal = QVector3D(normalTransform(0, 0) * normal.x() + normalTransform(0, 1) * normal.y() +
                               normalTransform(0, 2) * normal.z(),
                           normalTransform(1, 0) * normal.x() + normalTransform(1, 1) * normal.y() +
                               normalTransform(1, 2) * normal.z(),
                           normalTransform(2, 0) * normal.x() + normalTransform(2, 1) * normal.y() +
                               normalTransform(2, 2) * normal.z());
        float textureU = 0.0F;
        float textureV = 0.0F;
        if (textureCoordinateIndex >= 0) {
            const char *textureData =
                textureCoordinates.data + static_cast<qint64>(index) * textureCoordinates.stride;
            textureU = littleFloat(textureData);
            textureV = littleFloat(textureData + 4);
        }
        const float normalLengthSquared = normal.lengthSquared();
        if (!std::isfinite(transformedPosition.x()) || !std::isfinite(transformedPosition.y()) ||
            !std::isfinite(transformedPosition.z()) || !std::isfinite(normal.x()) ||
            !std::isfinite(normal.y()) || !std::isfinite(normal.z()) ||
            !std::isfinite(normalLengthSquared) || normalLengthSquared < 1.0e-12F ||
            !std::isfinite(textureU) || !std::isfinite(textureV)) {
            return false;
        }
        normal.normalize();
        if (!std::isfinite(normal.x()) || !std::isfinite(normal.y()) ||
            !std::isfinite(normal.z())) {
            return false;
        }
        mesh->vertices.insert(mesh->vertices.end(),
                              {transformedPosition.x(), transformedPosition.y(),
                               transformedPosition.z(), normal.x(), normal.y(), normal.z(),
                               textureU, textureV});
    }

    const std::size_t firstIndex = mesh->indices.size();
    for (int index = 0; index < indexCount; ++index) {
        quint32 local = static_cast<quint32>(index);
        if (indicesIndex >= 0) {
            const char *indexData = indices.data + static_cast<qint64>(index) * indices.stride;
            switch (indices.componentType) {
            case 5121:
                local = static_cast<quint32>(*reinterpret_cast<const unsigned char *>(indexData));
                break;
            case 5123:
                local = littleU16(indexData);
                break;
            case 5125:
                local = littleU32(indexData);
                break;
            default:
                return false;
            }
        }
        if (local >= static_cast<quint32>(positions.count) ||
            baseVertex + local > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        mesh->indices.push_back(static_cast<std::uint32_t>(baseVertex + local));
    }
    mesh->draws.push_back({firstIndex, static_cast<std::size_t>(indexCount), material.baseColor,
                           material.baseColorTexture, material.doubleSided});
    return true;
}

bool appendMesh(const ReaderContext &context, int meshIndex, const QMatrix4x4 &transform,
                PlaneModelMesh *mesh) {
    if (meshIndex < 0 || meshIndex >= context.meshes.size()) {
        return false;
    }
    const QJsonArray primitives =
        context.meshes[meshIndex].toObject().value(QStringLiteral("primitives")).toArray();
    if (primitives.isEmpty()) {
        return false;
    }
    for (const QJsonValue &primitive : primitives) {
        if (!primitive.isObject() ||
            !appendPrimitive(context, primitive.toObject(), transform, mesh)) {
            return false;
        }
    }
    return true;
}

bool flattenScene(const ReaderContext &context, const QJsonArray &roots, PlaneModelMesh *mesh) {
    // glTF nodes should form a forest. Track the active path to reject cycles
    // and all completed nodes to reject aliases that would duplicate geometry;
    // the depth cap also bounds recursive processing of hostile input.
    std::vector<bool> visiting(static_cast<std::size_t>(context.nodes.size()), false);
    std::vector<bool> visited(static_cast<std::size_t>(context.nodes.size()), false);
    std::function<bool(int, const QMatrix4x4 &, int)> visit;
    visit = [&](int index, const QMatrix4x4 &parent, int depth) {
        if (index < 0 || index >= context.nodes.size() || depth > 128 ||
            visiting[static_cast<std::size_t>(index)] || visited[static_cast<std::size_t>(index)]) {
            return false;
        }
        visiting[static_cast<std::size_t>(index)] = true;
        const QJsonObject node = context.nodes[index].toObject();
        bool transformValid = false;
        const QMatrix4x4 world = parent * nodeTransform(node, &transformValid);
        if (!transformValid) {
            return false;
        }
        const int meshIndex = node.value(QStringLiteral("mesh")).toInt(-1);
        if (meshIndex >= 0 && !appendMesh(context, meshIndex, world, mesh)) {
            return false;
        }
        for (const QJsonValue &child : node.value(QStringLiteral("children")).toArray()) {
            if (!child.isDouble() || !visit(child.toInt(-1), world, depth + 1)) {
                return false;
            }
        }
        visiting[static_cast<std::size_t>(index)] = false;
        visited[static_cast<std::size_t>(index)] = true;
        return true;
    };
    for (const QJsonValue &root : roots) {
        QMatrix4x4 identity;
        if (!root.isDouble() || !visit(root.toInt(-1), identity, 0)) {
            return false;
        }
    }
    return !mesh->empty();
}

bool normalizeMesh(PlaneModelMesh *mesh) {
    QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());
    QVector3D maximum(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
                      -std::numeric_limits<float>::max());
    for (std::size_t offset = 0; offset + PlaneModelVertexStrideFloats - 1U < mesh->vertices.size();
         offset += PlaneModelVertexStrideFloats) {
        const QVector3D value(mesh->vertices[offset], mesh->vertices[offset + 1U],
                              mesh->vertices[offset + 2U]);
        minimum.setX(std::min(minimum.x(), value.x()));
        minimum.setY(std::min(minimum.y(), value.y()));
        minimum.setZ(std::min(minimum.z(), value.z()));
        maximum.setX(std::max(maximum.x(), value.x()));
        maximum.setY(std::max(maximum.y(), value.y()));
        maximum.setZ(std::max(maximum.z(), value.z()));
    }
    const QVector3D extent = maximum - minimum;
    const float largest = std::max({extent.x(), extent.y(), extent.z()});
    if (!std::isfinite(largest) || largest <= 1.0e-6F) {
        return false;
    }
    // Plane rendering operates in scene-relative units, independent of the
    // model author's scale or origin. Center the AABB and make its largest axis
    // exactly two units while preserving aspect ratio.
    const QVector3D center = minimum + extent * 0.5F;
    const float scale = 2.0F / largest;
    mesh->forwardExtentSceneUnits = extent.z() * scale;
    if (!std::isfinite(mesh->forwardExtentSceneUnits) || mesh->forwardExtentSceneUnits <= 1.0e-6F) {
        return false;
    }
    for (std::size_t offset = 0; offset + PlaneModelVertexStrideFloats - 1U < mesh->vertices.size();
         offset += PlaneModelVertexStrideFloats) {
        mesh->vertices[offset] = (mesh->vertices[offset] - center.x()) * scale;
        mesh->vertices[offset + 1U] = (mesh->vertices[offset + 1U] - center.y()) * scale;
        mesh->vertices[offset + 2U] = (mesh->vertices[offset + 2U] - center.z()) * scale;
        if (!std::isfinite(mesh->vertices[offset]) || !std::isfinite(mesh->vertices[offset + 1U]) ||
            !std::isfinite(mesh->vertices[offset + 2U])) {
            return false;
        }
    }
    return true;
}

GlbModelReadResult parseModel(const QJsonObject &root, const QByteArray &glbBinary,
                              bool hasGlbBinary, const QString &sourceDirectory) {
    if (root.value(QStringLiteral("asset"))
                .toObject()
                .value(QStringLiteral("version"))
                .toString() != QStringLiteral("2.0") ||
        !root.value(QStringLiteral("extensionsRequired")).toArray().isEmpty()) {
        return failure(QStringLiteral("The selected glTF model requires unsupported extensions."));
    }
    const QJsonArray scenes = root.value(QStringLiteral("scenes")).toArray();
    const int sceneIndex = root.value(QStringLiteral("scene")).toInt(0);
    if (sceneIndex < 0 || sceneIndex >= scenes.size() || !scenes[sceneIndex].isObject()) {
        return failure(QStringLiteral("The selected glTF model has no valid default scene."));
    }

    ReaderContext context{root.value(QStringLiteral("accessors")).toArray(),
                          root.value(QStringLiteral("bufferViews")).toArray(),
                          root.value(QStringLiteral("materials")).toArray(),
                          root.value(QStringLiteral("meshes")).toArray(),
                          root.value(QStringLiteral("nodes")).toArray(),
                          root.value(QStringLiteral("images")).toArray(),
                          root.value(QStringLiteral("textures")).toArray(),
                          root.value(QStringLiteral("samplers")).toArray(),
                          {},
                          sourceDirectory};
    if (!lar::gltf::GlbResourceReader::resolveBuffers(
            root.value(QStringLiteral("buffers")).toArray(), sourceDirectory, glbBinary,
            hasGlbBinary, &context.binaries)) {
        return failure(QStringLiteral("The selected glTF model has invalid buffer resources."));
    }

    auto mesh = std::make_shared<PlaneModelMesh>();
    if (context.accessors.isEmpty() || context.bufferViews.isEmpty() || context.meshes.isEmpty() ||
        context.nodes.isEmpty() ||
        !GlbTextureReader::load(context.bufferViews, context.images, context.textures,
                                context.samplers, context.binaries, context.sourceDirectory,
                                mesh.get()) ||
        !flattenScene(context,
                      scenes[sceneIndex].toObject().value(QStringLiteral("nodes")).toArray(),
                      mesh.get()) ||
        !normalizeMesh(mesh.get())) {
        return failure(
            QStringLiteral("The selected glTF model exceeds the supported static mesh subset."));
    }
    return {std::move(mesh), {}};
}

GlbModelReadResult readModelFile(const QString &path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 ||
        file.size() > lar::gltf::limits::MaximumModelBytes) {
        return failure(QStringLiteral("The selected glTF model is unavailable or too large."));
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() != file.size()) {
        return failure(QStringLiteral("The selected glTF model could not be read completely."));
    }

    QByteArray jsonBytes = bytes;
    QByteArray binaryBytes;
    bool hasBinary = false;
    const bool glbFile = lar::gltf::GlbResourceReader::isGlb(bytes, suffix);
    if (glbFile) {
        lar::gltf::GlbContainer container;
        if (!lar::gltf::GlbResourceReader::splitGlb(bytes, &container)) {
            return failure(QStringLiteral("The selected file is not a valid GLB 2.0 model."));
        }
        jsonBytes = std::move(container.json);
        binaryBytes = std::move(container.binary);
        hasBinary = container.hasBinary;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(QStringLiteral("The selected glTF JSON is invalid."));
    }
    return parseModel(document.object(), binaryBytes, hasBinary, QFileInfo(path).absolutePath());
}

} // namespace

GlbModelReadResult GlbModelReader::readFile(const QString &path) {
    try {
        return readModelFile(path);
    } catch (const std::bad_alloc &) {
        return failure(QStringLiteral("The selected glTF model exhausted its memory budget."));
    }
}
