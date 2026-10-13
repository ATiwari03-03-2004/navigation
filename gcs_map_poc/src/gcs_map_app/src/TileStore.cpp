#include "TileStore.h"

#include <QFileInfo>
#include <QPainter>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QThread>

namespace {
const char *kProbeConnection = "tilestore_probe";
constexpr int kMaxOverzoom = 6;   // enlarge a tile up to 6 zoom levels (64x)

// Open a SQLite file read-only under the given connection name.
QSqlDatabase openReadOnly(const QString &connectionName, const QString &path)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    db.open();
    return db;
}
} // namespace

TileStore::TileStore(QObject *parent)
    : QObject(parent)
{
    m_placeholder = makePlaceholder();
}

QString TileStore::layout() const
{
    switch (m_layout) {
    case Layout::MapTilesDb: return QStringLiteral("maptiles.db (GCS tile database)");
    case Layout::MBTiles:    return QStringLiteral("MBTiles pack");
    case Layout::None:       break;
    }
    return QStringLiteral("none");
}

bool TileStore::open(const QString &path)
{
    m_path = path;
    m_layout = Layout::None;
    m_error.clear();

    if (path.isEmpty() || !QFileInfo::exists(path)) {
        m_error = QStringLiteral("Tile file not found: %1").arg(path.isEmpty() ? QStringLiteral("(none given)") : path);
        Q_EMIT changed();
        return false;
    }

    {
        QSqlDatabase db = openReadOnly(QLatin1String(kProbeConnection), path);
        if (!db.isOpen()) {
            m_error = QStringLiteral("Cannot open %1: %2").arg(path, db.lastError().text());
        } else {
            const QStringList tables = db.tables();
            QSqlQuery q(db);

            if (tables.contains(QStringLiteral("tile_set")) && tables.contains(QStringLiteral("tile"))
                && tables.contains(QStringLiteral("set_tile"))) {
                // --- maptiles.db (HLD schema): show the default tile set ---
                q.exec(QStringLiteral(
                    "SELECT set_id, name, attribution, min_zoom, max_zoom, north, south, east, west, tile_count "
                    "FROM tile_set ORDER BY is_default DESC, set_id LIMIT 1"));
                if (q.next()) {
                    m_setId = q.value(0).toLongLong();
                    m_name = q.value(1).toString();
                    m_attribution = q.value(2).toString();
                    m_minZoom = q.value(3).toInt();
                    m_maxZoom = q.value(4).toInt();
                    m_centerLat = (q.value(5).toDouble() + q.value(6).toDouble()) / 2.0;
                    m_centerLon = (q.value(7).toDouble() + q.value(8).toDouble()) / 2.0;
                    m_tileCount = q.value(9).toLongLong();
                    m_layout = Layout::MapTilesDb;
                } else {
                    m_error = QStringLiteral("maptiles.db has no tile sets. Import a pack with: ros2 run gcs_tile_tools import_mbtiles");
                }
            } else if (tables.contains(QStringLiteral("tiles"))) {
                // --- raw MBTiles pack ---
                if (tables.contains(QStringLiteral("metadata"))) {
                    q.exec(QStringLiteral("SELECT name, value FROM metadata"));
                    while (q.next()) {
                        const QString key = q.value(0).toString();
                        const QString value = q.value(1).toString();
                        if (key == QLatin1String("name"))
                            m_name = value;
                        else if (key == QLatin1String("attribution"))
                            m_attribution = value;
                        else if (key == QLatin1String("minzoom"))
                            m_minZoom = value.toInt();
                        else if (key == QLatin1String("maxzoom"))
                            m_maxZoom = value.toInt();
                        else if (key == QLatin1String("bounds")) {
                            const QStringList b = value.split(QLatin1Char(','));   // west,south,east,north
                            if (b.size() == 4) {
                                m_centerLon = (b[0].toDouble() + b[2].toDouble()) / 2.0;
                                m_centerLat = (b[1].toDouble() + b[3].toDouble()) / 2.0;
                            }
                        }
                    }
                }
                if (q.exec(QStringLiteral("SELECT COUNT(*) FROM tiles")) && q.next())
                    m_tileCount = q.value(0).toLongLong();
                m_layout = Layout::MBTiles;
            } else {
                m_error = QStringLiteral("%1 is neither a maptiles.db nor an MBTiles file.").arg(path);
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(QLatin1String(kProbeConnection));

    if (m_name.isEmpty())
        m_name = QFileInfo(path).completeBaseName();
    Q_EMIT changed();
    return ready();
}

QSqlDatabase TileStore::threadConnection() const
{
    // One connection per thread, named after the thread. QSqlDatabase's
    // connection registry is thread-safe; each connection is only used by the
    // thread that created it.
    const QString name = QStringLiteral("tilestore_%1")
                             .arg(reinterpret_cast<quintptr>(QThread::currentThread()), 0, 16);
    if (QSqlDatabase::contains(name))
        return QSqlDatabase::database(name);
    return openReadOnly(name, m_path);
}

QByteArray TileStore::readTile(int z, int x, int y) const
{
    if (z < 0 || x < 0 || y < 0 || x >= (1 << z) || y >= (1 << z))
        return {};

    QSqlDatabase db = threadConnection();
    if (!db.isOpen())
        return {};

    QSqlQuery q(db);
    if (m_layout == Layout::MapTilesDb) {
        q.prepare(QStringLiteral(
            "SELECT t.tile_data FROM tile t JOIN set_tile s ON s.tile_id = t.tile_id "
            "WHERE s.set_id = ? AND t.zoom = ? AND t.x = ? AND t.y = ?"));
        q.addBindValue(m_setId);
        q.addBindValue(z);
        q.addBindValue(x);
        q.addBindValue(y);
    } else {
        // MBTiles counts rows from the south (TMS): row = 2^z - 1 - y.
        q.prepare(QStringLiteral(
            "SELECT tile_data FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?"));
        q.addBindValue(z);
        q.addBindValue(x);
        q.addBindValue((1 << z) - 1 - y);
    }
    if (q.exec() && q.next())
        return q.value(0).toByteArray();
    return {};
}

QImage TileStore::loadTile(int z, int x, int y) const
{
    if (!ready())
        return m_placeholder;

    // 1. The tile itself.
    QImage img;
    const QByteArray data = readTile(z, x, y);
    if (!data.isEmpty() && img.loadFromData(data))
        return img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // 2. Overzoom: crop and enlarge the nearest lower-zoom tile that covers this area.
    for (int dz = 1; dz <= kMaxOverzoom && z - dz >= 0; ++dz) {
        const int pz = z - dz;
        const int px = x >> dz;
        const int py = y >> dz;
        const QByteArray pdata = readTile(pz, px, py);
        QImage parent;
        if (pdata.isEmpty() || !parent.loadFromData(pdata))
            continue;
        const int sub = parent.width() >> dz;          // size of our part, in parent pixels
        if (sub < 1)
            break;
        const int ox = (x - (px << dz)) * sub;
        const int oy = (y - (py << dz)) * sub;
        return parent.copy(ox, oy, sub, sub)
            .scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    // 3. Nothing covers this area.
    return m_placeholder;
}

QImage TileStore::makePlaceholder() const
{
    QImage img(256, 256, QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(0xd6, 0xd9, 0xdd));
    QPainter p(&img);
    p.setPen(QPen(QColor(0xc4, 0xc8, 0xcd), 1));
    for (int i = 0; i <= 256; i += 32) {
        p.drawLine(i, 0, i, 256);
        p.drawLine(0, i, 256, i);
    }
    p.setPen(QColor(0x8a, 0x90, 0x98));
    p.drawText(img.rect(), Qt::AlignCenter, QStringLiteral("No map tiles here"));
    return img;
}
