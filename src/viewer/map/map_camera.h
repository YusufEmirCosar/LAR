#pragma once

/**
 * @file map_camera.h
 * @brief Projection-independent Earth map camera model and hit testing.
 */

#include <QMatrix4x4>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QVector2D>

namespace lar::map {

/** @brief Flat Web-Mercator or orthographic sphere presentation. */
enum class MapPresentation { Mercator, Sphere };

/** @brief Visible projected bounds centered on the current flat camera. */
struct MercatorViewport final {
    double halfWidth = 0.0;
    double halfHeight = 0.0;
    double left = 0.0;
    double right = 0.0;
    double bottom = 0.0;
    double top = 0.0;
};

/** @brief Inclusive periodic world-copy indices visible in a flat viewport. */
struct WorldCopyRange final {
    int first = 0;
    int last = 0;

    [[nodiscard]] int count() const noexcept {
        return last >= first ? last - first + 1 : 0;
    }
};

/**
 * @brief Compensated single-precision inputs for stable sphere projection.
 *
 * The camera center is split into high and low float components so GPU shaders
 * can subtract it from geographic vertices without first rounding the tracked
 * double-precision coordinate to one float.
 */
struct SphereProjectionParameters final {
    QVector2D centerHighDegrees;
    QVector2D centerLowDegrees;
    QVector2D latitudeSinCos;
};

/**
 * @brief Stores camera state and performs projection, pan, zoom, and picking.
 *
 * Geographic inputs and bearings are degrees. Flat-map coordinates use
 * longitude degrees for x and `MapProjection::projectLatitude()` projected
 * degrees for y; longitude repeats every 360 degrees. Screen coordinates are
 * pixels with the origin at the upper-left.
 *
 * Unless a method documents another meaning, a boolean mutator reports whether
 * the effective stored value changed after normalization or clamping. Projection
 * methods separate input validity from visibility: a valid geographic point can
 * project successfully while lying outside the viewport or on the sphere's back
 * hemisphere.
 */
class MapCamera final {
  public:
    [[nodiscard]] MapPresentation presentation() const noexcept;
    bool setPresentation(MapPresentation presentation) noexcept;

    /** Returns `(longitude, latitude)` in geographic degrees. */
    [[nodiscard]] QPointF sphereCenter() const noexcept;
    [[nodiscard]] SphereProjectionParameters sphereProjectionParameters() const noexcept;
    bool setSphereCenter(double longitudeDegrees, double latitudeDegrees) noexcept;

    [[nodiscard]] float bearingDegrees() const noexcept;
    bool setBearingDegrees(float bearingDegrees) noexcept;

    /** Returns the finite projected-degree bounds used for flat-map constraints. */
    [[nodiscard]] QRectF mercatorWorldBounds() const noexcept;
    bool setMercatorWorldBounds(const QRectF &projectedBounds, int viewportWidth,
                                int viewportHeight) noexcept;

    [[nodiscard]] QPointF mercatorCenter() const noexcept;
    bool setMercatorCenter(const QPointF &projectedCenter, int viewportWidth,
                           int viewportHeight) noexcept;

    [[nodiscard]] float mercatorZoom() const noexcept;
    bool setMercatorZoom(float zoom, int viewportWidth, int viewportHeight) noexcept;

    [[nodiscard]] float sphereZoom() const noexcept;
    bool setSphereZoom(float zoom) noexcept;

    [[nodiscard]] float activeZoom() const noexcept;

    void setGeometryLongitudeBounds(double minimumLongitude, double maximumLongitude) noexcept;

    [[nodiscard]] MercatorViewport mercatorViewport(int viewportWidth,
                                                    int viewportHeight) const noexcept;
    /** Returns inclusive 360-degree copy indices intersecting the rotated viewport. */
    [[nodiscard]] WorldCopyRange visibleWorldCopies(int viewportWidth,
                                                    int viewportHeight) const noexcept;
    [[nodiscard]] WorldCopyRange visibleWorldCopiesForBounds(double minimumLongitude,
                                                             double maximumLongitude,
                                                             int viewportWidth,
                                                             int viewportHeight) const noexcept;
    /** Converts upper-left-origin screen pixels to unwrapped projected degrees. */
    [[nodiscard]] QPointF screenToMercator(const QPointF &screenPosition, int viewportWidth,
                                           int viewportHeight) const noexcept;

    bool panMercator(const QPoint &pixelDelta, int viewportWidth, int viewportHeight) noexcept;
    bool zoomAt(const QPointF &screenPosition, int wheelDelta, int viewportWidth,
                int viewportHeight) noexcept;
    void constrainMercatorCenter(int viewportWidth, int viewportHeight) noexcept;

    [[nodiscard]] QMatrix4x4 projectionMatrix(int viewportWidth, int viewportHeight) const noexcept;
    /**
     * Projects a valid geographic coordinate and reports viewport and front
     * hemisphere membership together through `visible`.
     */
    [[nodiscard]] bool projectGeoToScreen(double longitudeDegrees, double latitudeDegrees,
                                          int viewportWidth, int viewportHeight,
                                          QPointF &screenPosition, bool &visible) const noexcept;

    /** Inverts orthographic projection for points on the visible sphere disk. */
    [[nodiscard]] bool sphereCoordinateAt(const QPointF &screenPosition, int viewportWidth,
                                          int viewportHeight, double &longitudeDegrees,
                                          double &latitudeDegrees) const noexcept;

  private:
    void normalizeMercatorCenter() noexcept;
    void clampMercatorCenterY(int viewportWidth, int viewportHeight) noexcept;

    MapPresentation m_presentation = MapPresentation::Mercator;
    QRectF m_mercatorWorldBounds{-180.0, -180.0, 360.0, 360.0};
    QPointF m_mercatorCenter{0.0, 0.0};
    float m_mercatorZoom = 1.0F;
    float m_sphereZoom = 1.0F;
    double m_sphereLongitude = 0.0;
    double m_sphereLatitude = 0.0;
    float m_bearingDegrees = 0.0F;
    double m_geometryMinimumLongitude = -180.0;
    double m_geometryMaximumLongitude = 180.0;
};

} // namespace lar::map
