// Web Mercator (EPSG:3857) helpers for the tile tools.
// Same formulas as gcs_map_app/src/GeoMath.h, so the test tiles check the app's maths.
#pragma once

#include <algorithm>
#include <cmath>

namespace tiles {

constexpr double kPi = 3.14159265358979323846;
constexpr int kTileSize = 256;
constexpr double kMaxLat = 85.05112878;

inline double worldSize(int z) { return kTileSize * std::ldexp(1.0, z); }

inline double lonToX(double lon, int z) { return (lon + 180.0) / 360.0 * worldSize(z); }

inline double latToY(double lat, int z)
{
    const double r = std::clamp(lat, -kMaxLat, kMaxLat) * kPi / 180.0;
    return (1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / kPi) / 2.0 * worldSize(z);
}

inline double xToLon(double x, int z) { return x / worldSize(z) * 360.0 - 180.0; }

inline double yToLat(double y, int z)
{
    const double n = kPi - 2.0 * kPi * y / worldSize(z);
    return std::atan(std::sinh(n)) * 180.0 / kPi;
}

inline double metersPerPixel(double lat, int z)
{
    return 2.0 * kPi * 6378137.0 * std::cos(lat * kPi / 180.0) / worldSize(z);
}

// MBTiles stores rows from the south (TMS); XYZ counts from the north.
inline int flipRow(int z, int row) { return (1 << z) - 1 - row; }

} // namespace tiles
