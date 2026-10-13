// Web Mercator (EPSG:3857) helpers used by the map.
//
// "World pixels" are pixel coordinates on the whole-world map at zoom z:
// the world is 256 * 2^z pixels wide and tall, x grows east, y grows south.
// Tile (x, y) at zoom z covers world pixels [x*256, x*256+256) x [y*256, y*256+256).
#pragma once

#include <algorithm>
#include <cmath>

namespace geo {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTileSize = 256.0;
constexpr double kMaxLat = 85.05112878;           // Web Mercator latitude limit
constexpr double kEarthRadiusM = 6378137.0;        // WGS 84 equatorial radius

inline double degToRad(double d) { return d * kPi / 180.0; }
inline double radToDeg(double r) { return r * 180.0 / kPi; }

inline double worldSize(int zoom) { return kTileSize * std::ldexp(1.0, zoom); }

inline double lonToWorldX(double lon, int zoom)
{
    return (lon + 180.0) / 360.0 * worldSize(zoom);
}

inline double latToWorldY(double lat, int zoom)
{
    const double r = degToRad(std::clamp(lat, -kMaxLat, kMaxLat));
    return (1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / kPi) / 2.0 * worldSize(zoom);
}

inline double worldXToLon(double x, int zoom)
{
    return x / worldSize(zoom) * 360.0 - 180.0;
}

inline double worldYToLat(double y, int zoom)
{
    const double n = kPi - 2.0 * kPi * y / worldSize(zoom);
    return radToDeg(std::atan(std::sinh(n)));
}

// Ground metres covered by one screen pixel at this latitude and zoom.
inline double metersPerPixel(double lat, int zoom)
{
    return 2.0 * kPi * kEarthRadiusM * std::cos(degToRad(lat)) / worldSize(zoom);
}

// Great-circle distance in metres (haversine).
inline double distanceM(double lat1, double lon1, double lat2, double lon2)
{
    const double dLat = degToRad(lat2 - lat1);
    const double dLon = degToRad(lon2 - lon1);
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
                   + std::cos(degToRad(lat1)) * std::cos(degToRad(lat2))
                   * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2.0 * 6371000.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

} // namespace geo
