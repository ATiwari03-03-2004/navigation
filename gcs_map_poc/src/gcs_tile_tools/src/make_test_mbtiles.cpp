// make_test_mbtiles: draws a synthetic offline tile pack (.mbtiles) for the map POC.
//
// No internet and no map data needed. Every tile is drawn from scratch:
//   - a latitude/longitude grid with labels (the spacing adapts to the zoom),
//   - rings at 100 m, 250 m, 500 m, 1 km and 2 km around the start point,
//   - the start point itself,
//   - the tile's own z/x/y address in the corner.
//
// The grid uses the same Web Mercator maths as the GCS app, so it doubles as a
// correctness check: the robot's latitude/longitude in the side panel should
// match the grid labels under its marker.
//
// Usage:
//   ros2 run gcs_tile_tools make_test_mbtiles --out maps/test_area.mbtiles
//   ros2 run gcs_tile_tools make_test_mbtiles --lat 12.9716 --lon 77.5946 --radius-km 3

#include <cmath>
#include <cstdio>

#include <QBuffer>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "TileMath.h"

using namespace tiles;

namespace {

// Grid spacing in degrees so that lines are roughly 100-250 px apart.
double gridStepForZoom(int z)
{
    static const double steps[] = {0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02,
                                   0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0};
    const double degPerPx = 360.0 / worldSize(z);
    for (double s : steps)
        if (s / degPerPx >= 100.0)
            return s;
    return 10.0;
}

QString fmtDeg(double v, double step)
{
    const int decimals = std::max(0, -static_cast<int>(std::floor(std::log10(step))));
    return QString::number(v, 'f', decimals);
}

QByteArray drawTile(int z, int tx, int ty, double startLat, double startLon)
{
    QImage img(kTileSize, kTileSize, QImage::Format_RGB32);
    img.fill(QColor(242, 239, 233));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    QFont small = p.font();
    small.setPixelSize(11);
    QFont label = p.font();
    label.setPixelSize(12);

    const double ox = double(tx) * kTileSize;   // world pixel of the tile's top-left corner
    const double oy = double(ty) * kTileSize;

    // Light checkerboard so tile edges are visible while panning.
    if ((tx + ty) % 2 == 0)
        p.fillRect(img.rect(), QColor(236, 233, 226));

    // Lat/lon grid
    const double step = gridStepForZoom(z);
    const double lonW = xToLon(ox, z), lonE = xToLon(ox + kTileSize, z);
    const double latN = yToLat(oy, z), latS = yToLat(oy + kTileSize, z);
    p.setFont(small);
    for (double lon = std::floor(lonW / step) * step; lon <= lonE + 1e-12; lon += step) {
        const double px = lonToX(lon, z) - ox;
        if (px < 0 || px >= kTileSize)
            continue;
        p.setPen(QPen(QColor(160, 170, 185), 1));
        p.drawLine(QPointF(px, 0), QPointF(px, kTileSize));
        p.setPen(QColor(90, 100, 115));
        p.drawText(QPointF(px + 3, kTileSize - 4), fmtDeg(lon, step) + QStringLiteral("°E"));
    }
    for (double lat = std::floor(latS / step) * step; lat <= latN + 1e-12; lat += step) {
        const double py = latToY(lat, z) - oy;
        if (py < 0 || py >= kTileSize)
            continue;
        p.setPen(QPen(QColor(160, 170, 185), 1));
        p.drawLine(QPointF(0, py), QPointF(kTileSize, py));
        p.setPen(QColor(90, 100, 115));
        p.drawText(QPointF(3, py + 13), fmtDeg(lat, step) + QStringLiteral("°N"));
    }

    // Distance rings around the start point
    const double sx = lonToX(startLon, z) - ox;
    const double sy = latToY(startLat, z) - oy;
    const double mpp = metersPerPixel(startLat, z);
    struct Ring { int m; QColor c; };
    const Ring rings[] = {{100, QColor(214, 120, 60)}, {250, QColor(200, 150, 60)},
                          {500, QColor(120, 160, 90)}, {1000, QColor(80, 140, 190)},
                          {2000, QColor(130, 110, 190)}};
    p.setFont(label);
    for (const Ring &r : rings) {
        const double rad = r.m / mpp;
        if (rad < 6)
            continue;
        if (sx + rad < 0 || sx - rad > kTileSize || sy + rad < 0 || sy - rad > kTileSize)
            continue;
        p.setPen(QPen(r.c, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(sx, sy), rad, rad);
        p.drawText(QPointF(sx + 4, sy - rad + 14),
                   r.m < 1000 ? QStringLiteral("%1 m").arg(r.m) : QStringLiteral("%1 km").arg(r.m / 1000));
    }

    // Start point
    if (sx >= -10 && sx <= kTileSize + 10 && sy >= -10 && sy <= kTileSize + 10) {
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(QColor(220, 50, 50));
        p.drawEllipse(QPointF(sx, sy), 6, 6);
        p.setPen(QColor(200, 40, 40));
        p.drawText(QPointF(sx + 9, sy + 5), QStringLiteral("START"));
    }

    // Tile border and address
    p.setBrush(Qt::NoBrush);
    p.setPen(QColor(215, 211, 203));
    p.drawRect(0, 0, kTileSize - 1, kTileSize - 1);
    p.setFont(small);
    p.setPen(QColor(150, 150, 150));
    p.drawText(QPointF(4, 13), QStringLiteral("%1/%2/%3").arg(z).arg(tx).arg(ty));
    p.end();

    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return png;
}

bool exec(QSqlQuery &q, const QString &sql)
{
    if (!q.exec(sql)) {
        std::fprintf(stderr, "SQL error: %s\n  in: %s\n", qPrintable(q.lastError().text()), qPrintable(sql));
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    // Draw text without a display (works over SSH, in CI, etc.).
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Draw a synthetic offline tile pack (.mbtiles) for the GCS map POC."));
    parser.addHelpOption();
    QCommandLineOption latOpt("lat", "Centre latitude (default 28.6129).", "deg", "28.6129");
    QCommandLineOption lonOpt("lon", "Centre longitude (default 77.2295).", "deg", "77.2295");
    QCommandLineOption radiusOpt("radius-km", "Half-width of the area in km (default 2).", "km", "2");
    QCommandLineOption minZOpt("min-zoom", "Lowest zoom (default 12).", "z", "12");
    QCommandLineOption maxZOpt("max-zoom", "Highest zoom (default 18).", "z", "18");
    QCommandLineOption nameOpt("name", "Tile set name.", "name", "POC test area");
    QCommandLineOption outOpt("out", "Output .mbtiles file.", "file", "maps/test_area.mbtiles");
    parser.addOptions({latOpt, lonOpt, radiusOpt, minZOpt, maxZOpt, nameOpt, outOpt});
    parser.process(app);

    const double lat = parser.value(latOpt).toDouble();
    const double lon = parser.value(lonOpt).toDouble();
    const double radiusKm = parser.value(radiusOpt).toDouble();
    const int minZ = parser.value(minZOpt).toInt();
    const int maxZ = parser.value(maxZOpt).toInt();
    const QString out = parser.value(outOpt);
    if (!(0 <= minZ && minZ <= maxZ && maxZ <= 20)) {
        std::fprintf(stderr, "Zoom levels must satisfy 0 <= min-zoom <= max-zoom <= 20\n");
        return 1;
    }

    const double dLat = radiusKm / 110.574;
    const double dLon = radiusKm / (111.320 * std::cos(lat * kPi / 180.0));
    const double north = lat + dLat, south = lat - dLat, east = lon + dLon, west = lon - dLon;

    QDir().mkpath(QFileInfo(out).absolutePath());
    QFile::remove(out);

    int total = 0;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "out");
        db.setDatabaseName(out);
        if (!db.open()) {
            std::fprintf(stderr, "Cannot create %s: %s\n", qPrintable(out), qPrintable(db.lastError().text()));
            return 1;
        }
        QSqlQuery q(db);
        if (!exec(q, "CREATE TABLE metadata (name TEXT, value TEXT)")
            || !exec(q, "CREATE TABLE tiles (zoom_level INTEGER, tile_column INTEGER, tile_row INTEGER, tile_data BLOB)")
            || !exec(q, "CREATE UNIQUE INDEX tile_index ON tiles (zoom_level, tile_column, tile_row)")
            || !exec(q, "CREATE UNIQUE INDEX name ON metadata (name)"))
            return 1;

        const QList<QPair<QString, QString>> meta = {
            {"name", parser.value(nameOpt)},
            {"format", "png"},
            {"type", "baselayer"},
            {"version", "1"},
            {"description", "Synthetic grid tiles for the GCS map POC (no real map data)"},
            {"attribution", "Synthetic test tiles - no real map data"},
            {"bounds", QStringLiteral("%1,%2,%3,%4").arg(west, 0, 'f', 6).arg(south, 0, 'f', 6)
                           .arg(east, 0, 'f', 6).arg(north, 0, 'f', 6)},
            {"center", QStringLiteral("%1,%2,%3").arg(lon, 0, 'f', 6).arg(lat, 0, 'f', 6).arg(std::min(17, maxZ))},
            {"minzoom", QString::number(minZ)},
            {"maxzoom", QString::number(maxZ)},
        };
        q.prepare("INSERT INTO metadata (name, value) VALUES (?, ?)");
        for (const auto &kv : meta) {
            q.addBindValue(kv.first);
            q.addBindValue(kv.second);
            q.exec();
        }

        QElapsedTimer timer;
        timer.start();
        for (int z = minZ; z <= maxZ; ++z) {
            const int x0 = int(lonToX(west, z) / kTileSize), x1 = int(lonToX(east, z) / kTileSize);
            const int y0 = int(latToY(north, z) / kTileSize), y1 = int(latToY(south, z) / kTileSize);
            int count = 0;
            db.transaction();
            QSqlQuery ins(db);
            ins.prepare("INSERT INTO tiles (zoom_level, tile_column, tile_row, tile_data) VALUES (?, ?, ?, ?)");
            for (int tx = x0; tx <= x1; ++tx) {
                for (int ty = y0; ty <= y1; ++ty) {
                    ins.addBindValue(z);
                    ins.addBindValue(tx);
                    ins.addBindValue(flipRow(z, ty));   // MBTiles rows count from the south (TMS)
                    ins.addBindValue(drawTile(z, tx, ty, lat, lon));
                    if (!ins.exec()) {
                        std::fprintf(stderr, "Insert failed: %s\n", qPrintable(ins.lastError().text()));
                        db.rollback();
                        return 1;
                    }
                    ++count;
                }
            }
            db.commit();
            total += count;
            std::printf("  zoom %2d: %5d tiles  (x %d-%d, y %d-%d)\n", z, count, x0, x1, y0, y1);
        }
        db.close();
        std::printf("Wrote %d tiles, %.1f MB, to %s in %.1f s\n", total, QFileInfo(out).size() / 1e6,
                    qPrintable(out), timer.elapsed() / 1000.0);
        std::printf("Area: %.5f..%.5f N, %.5f..%.5f E, zoom %d-%d\n", south, north, west, east, minZ, maxZ);
    }
    QSqlDatabase::removeDatabase("out");
    return 0;
}
