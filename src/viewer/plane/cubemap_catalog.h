#pragma once

/**
 * @file cubemap_catalog.h
 * @brief Discovery and validation of packaged six-file cubemap sets.
 */

#include <QImage>
#include <QString>
#include <QVector>

#include <array>

struct CubemapFaces final {
    // OpenGL cubemap order: +X, -X, +Y, -Y, +Z, -Z.
    std::array<QImage, 6> images;
    QString displayName;
};

/** @brief Naturally ordered collection of validated skybox candidates. */
class CubemapCatalog final {
  public:
    explicit CubemapCatalog(QString directory = {});

    [[nodiscard]] int count() const noexcept {
        return static_cast<int>(m_entries.size());
    }
    [[nodiscard]] int rejectedSetCount() const noexcept {
        return m_rejectedSetCount;
    }
    [[nodiscard]] QString displayName(int index) const;
    [[nodiscard]] bool load(int index, CubemapFaces *faces, QString *errorMessage = nullptr) const;

  private:
    struct Entry final {
        std::array<QString, 6> facePaths;
        QString name;
    };
    QVector<Entry> m_entries;
    int m_rejectedSetCount = 0;
};
