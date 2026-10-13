#include "MapController.h"

#include <algorithm>
#include <cmath>

#include <QVariantMap>

#include "GeoMath.h"

namespace {
constexpr double kTrailMinStepM = 1.0;   // add a trail point after moving at least 1 m (HLD 5.5)
constexpr size_t kTrailMaxPoints = 5000; // keep memory bounded: oldest points are dropped
constexpr double kScaleBarMaxPx = 120.0;
}

MapController::MapController(int minZoom, int maxZoom, int startZoom,
                             double startLat, double startLon, QObject *parent)
    : QObject(parent)
    , m_minZoom(minZoom)
    , m_maxZoom(std::max(minZoom, maxZoom))
    , m_zoom(std::clamp(startZoom, m_minZoom, m_maxZoom))
{
    m_centerX = geo::lonToWorldX(startLon, m_zoom);
    m_centerY = geo::latToWorldY(startLat, m_zoom);
    updateScaleBar();
}

double MapController::centerLat() const { return geo::worldYToLat(m_centerY, m_zoom); }
double MapController::centerLon() const { return geo::worldXToLon(m_centerX, m_zoom); }

void MapController::setFollow(bool follow)
{
    if (m_follow == follow)
        return;
    m_follow = follow;
    Q_EMIT followChanged();
    if (m_follow && m_hasRobot)
        recenter();
}

// ---------------------------------------------------------------- view

void MapController::setViewportSize(double width, double height)
{
    if (width <= 0 || height <= 0)
        return;
    m_viewW = width;
    m_viewH = height;
    updateView(true);
}

void MapController::pan(double dx, double dy)
{
    if (dx == 0.0 && dy == 0.0)
        return;
    // Dragging the map stops following the robot (HLD 5.5: Following -> FreePan).
    if (m_follow) {
        m_follow = false;
        Q_EMIT followChanged();
    }
    m_centerX -= dx;
    m_centerY -= dy;
    updateView();
}

void MapController::zoomIn() { zoomAt(1, m_viewW / 2.0, m_viewH / 2.0); }
void MapController::zoomOut() { zoomAt(-1, m_viewW / 2.0, m_viewH / 2.0); }

void MapController::zoomAt(int steps, double screenX, double screenY)
{
    // While following, always zoom around the robot (the view centre).
    if (m_follow) {
        screenX = m_viewW / 2.0;
        screenY = m_viewH / 2.0;
    }
    setZoom(m_zoom + steps, screenX, screenY);
}

void MapController::setZoom(int zoom, double anchorX, double anchorY)
{
    zoom = std::clamp(zoom, m_minZoom, m_maxZoom);
    if (zoom == m_zoom)
        return;
    // World pixel under the anchor point, before and after: worldPx(z+1) = 2 * worldPx(z).
    const double ax = m_centerX - m_viewW / 2.0 + anchorX;
    const double ay = m_centerY - m_viewH / 2.0 + anchorY;
    const double f = std::ldexp(1.0, zoom - m_zoom);
    m_centerX = ax * f - (anchorX - m_viewW / 2.0);
    m_centerY = ay * f - (anchorY - m_viewH / 2.0);
    m_zoom = zoom;
    updateView(true);
}

void MapController::recenter()
{
    if (!m_follow) {
        m_follow = true;
        Q_EMIT followChanged();
    }
    if (m_hasRobot) {
        m_centerX = geo::lonToWorldX(m_robot.lon, m_zoom);
        m_centerY = geo::latToWorldY(m_robot.lat, m_zoom);
        updateView();
    }
}

void MapController::clearTrail()
{
    m_trail.clear();
    m_trailLengthM = 0.0;
    if (m_hasRobot)
        m_trail.push_back(m_robot);
    updateTrailScreen();
}

void MapController::updateView(bool forceTiles)
{
    if (m_viewW <= 0 || m_viewH <= 0)
        return;

    const double world = geo::worldSize(m_zoom);
    m_centerY = std::clamp(m_centerY, 0.0, world);
    // Longitude wraps around the world.
    m_centerX = std::fmod(std::fmod(m_centerX, world) + world, world);

    const int n = 1 << m_zoom;
    const double left = m_centerX - m_viewW / 2.0;
    const double top = m_centerY - m_viewH / 2.0;

    // Visible tiles plus a margin of one tile on each side, so a short drag
    // never shows an empty edge before the new tiles arrive.
    const int tx0 = std::max(0, static_cast<int>(std::floor(left / geo::kTileSize)) - 1);
    const int ty0 = std::max(0, static_cast<int>(std::floor(top / geo::kTileSize)) - 1);
    const int tx1 = std::min(n - 1, static_cast<int>(std::floor((left + m_viewW) / geo::kTileSize)) + 1);
    const int ty1 = std::min(n - 1, static_cast<int>(std::floor((top + m_viewH) / geo::kTileSize)) + 1);

    const bool rangeChanged = forceTiles || m_tileZoom != m_zoom
                              || tx0 != m_tx0 || ty0 != m_ty0 || tx1 != m_tx1 || ty1 != m_ty1;
    if (rangeChanged) {
        m_tileZoom = m_zoom;
        m_tx0 = tx0; m_ty0 = ty0; m_tx1 = tx1; m_ty1 = ty1;
        m_originX = tx0 * geo::kTileSize;
        m_originY = ty0 * geo::kTileSize;

        m_tiles.clear();
        for (int ty = ty0; ty <= ty1; ++ty) {
            for (int tx = tx0; tx <= tx1; ++tx) {
                QVariantMap t;
                t.insert(QStringLiteral("url"),
                         QStringLiteral("image://maptiles/%1/%2/%3").arg(m_zoom).arg(tx).arg(ty));
                t.insert(QStringLiteral("x"), (tx - tx0) * geo::kTileSize);
                t.insert(QStringLiteral("y"), (ty - ty0) * geo::kTileSize);
                m_tiles.append(t);
            }
        }
    }

    // Where the layer's origin sits on screen.
    m_layerX = m_viewW / 2.0 - (m_centerX - m_originX);
    m_layerY = m_viewH / 2.0 - (m_centerY - m_originY);

    if (rangeChanged) {
        // Robot and trail are relative to the origin: recompute them with the new origin.
        updateRobotScreen();
        updateTrailScreen();
        Q_EMIT tilesChanged();
    }
    updateScaleBar();
    Q_EMIT viewChanged();
}

void MapController::updateScaleBar()
{
    const double mpp = geo::metersPerPixel(centerLat(), m_zoom);
    static const double nice[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000,
                                  5000, 10000, 20000, 50000, 100000, 200000, 500000};
    double meters = nice[0];
    for (double d : nice) {
        if (d / mpp <= kScaleBarMaxPx)
            meters = d;
    }
    m_scalePx = meters / mpp;
    m_scaleLabel = meters >= 1000 ? QStringLiteral("%1 km").arg(qRound(meters / 1000.0))
                                  : QStringLiteral("%1 m").arg(qRound(meters));
}

// ---------------------------------------------------------------- robot

void MapController::onPosition(double lat, double lon, double headingDeg, bool headingValid, bool hasFix)
{
    if (!hasFix) {
        // No fix: keep the last good position on screen, but mark it (grey marker).
        if (m_robotHasFix) {
            m_robotHasFix = false;
            Q_EMIT robotChanged();
        }
        return;
    }

    const bool first = !m_hasRobot;
    m_robot = {lat, lon};
    m_hasRobot = true;
    m_robotHasFix = true;
    m_headingValid = headingValid;
    if (headingValid)
        m_heading = headingDeg;

    // Trail: add a point after moving at least kTrailMinStepM.
    if (m_trail.empty()) {
        m_trail.push_back(m_robot);
    } else {
        const GeoPoint &last = m_trail.back();
        const double d = geo::distanceM(last.lat, last.lon, lat, lon);
        if (d >= kTrailMinStepM) {
            m_trail.push_back(m_robot);
            m_trailLengthM += d;
        }
    }
    if (m_trail.size() > kTrailMaxPoints)
        m_trail.erase(m_trail.begin(), m_trail.begin() + (m_trail.size() - kTrailMaxPoints));

    if (m_follow || first) {
        // Keep the robot in the centre. updateView() also refreshes the robot and
        // the trail when the tile range (and so the origin) changes.
        m_centerX = geo::lonToWorldX(lon, m_zoom);
        m_centerY = geo::latToWorldY(lat, m_zoom);
        updateView();
    }
    updateRobotScreen();
    // The trail ends at the robot's current position, so redraw it on every
    // message, not only when a new point was sampled.
    updateTrailScreen();
}

void MapController::updateRobotScreen()
{
    if (!m_hasRobot)
        return;
    m_robotX = geo::lonToWorldX(m_robot.lon, m_zoom) - m_originX;
    m_robotY = geo::latToWorldY(m_robot.lat, m_zoom) - m_originY;
    Q_EMIT robotChanged();
}

void MapController::updateTrailScreen()
{
    m_trailScreen.clear();
    m_trailScreen.reserve(static_cast<int>(m_trail.size()));
    for (const GeoPoint &p : m_trail) {
        m_trailScreen.append(QPointF(geo::lonToWorldX(p.lon, m_zoom) - m_originX,
                                     geo::latToWorldY(p.lat, m_zoom) - m_originY));
    }
    // Draw the trail up to the robot's current position, not just the last sampled point.
    if (m_hasRobot && !m_trail.empty())
        m_trailScreen.append(QPointF(m_robotX, m_robotY));
    Q_EMIT trailChanged();
}
