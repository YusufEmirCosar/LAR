#pragma once

/**
 * @file glb_resource_reader.h
 * @brief Contained URI and bounded GLB-buffer resolution for static glTF models.
 */

#include <QByteArray>
#include <QJsonArray>
#include <QString>

#include <vector>

namespace lar::gltf {

/** @brief JSON and optional BIN chunks extracted from one validated GLB container. */
struct GlbContainer final {
    QByteArray json;
    QByteArray binary;
    bool hasBinary = false;
};

/**
 * @brief Resolves glTF resources without allowing a model package to escape its directory.
 *
 * External paths must be regular, non-symlink files canonically contained below the selected
 * model's directory. Data URIs and external files share the same per-resource byte ceiling.
 */
class GlbResourceReader final {
  public:
    /** @brief Returns whether @p bytes or @p suffix identify a binary GLB container. */
    [[nodiscard]] static bool isGlb(const QByteArray &bytes, const QString &suffix) noexcept;

    /** @brief Splits and validates a GLB 2.0 container without interpreting its JSON. */
    [[nodiscard]] static bool splitGlb(const QByteArray &bytes, GlbContainer *container);

    /**
     * @brief Reads one data URI or one contained external resource.
     *
     * @param[in] uri glTF URI to resolve.
     * @param[in] sourceDirectory Canonical package boundary for external resources.
     * @param[in] maximumBytes Maximum decoded/read bytes for this resource.
     * @param[out] result Complete resource bytes, committed only on success.
     */
    [[nodiscard]] static bool readUri(const QString &uri, const QString &sourceDirectory,
                                      qint64 maximumBytes, QByteArray *result);

    /**
     * @brief Resolves every declared glTF buffer under a single aggregate logical-byte budget.
     *
     * The aggregate is preflighted from declared `byteLength` values before any external file is
     * opened, so repeating one URI cannot multiply retained memory beyond the budget.
     */
    [[nodiscard]] static bool resolveBuffers(const QJsonArray &bufferTable,
                                             const QString &sourceDirectory,
                                             const QByteArray &glbBinary, bool hasGlbBinary,
                                             std::vector<QByteArray> *binaries);
};

} // namespace lar::gltf
