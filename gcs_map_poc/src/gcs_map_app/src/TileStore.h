// TileStore: the Map Tile Model. Reads offline map tiles from a local SQLite file.
//
// Two file layouts are accepted:
//   1. maptiles.db  - the GCS map tile database from the HLD (Section 4.2.3):
//                     tables tile_set, tile (XYZ rows) and set_tile.
//                     Made from an .mbtiles pack by gcs_tile_tools/import_mbtiles.
//   2. *.mbtiles    - a raw MBTiles 1.3 pack: tables metadata and tiles (TMS rows).
//
// Tiles are read on the image provider's worker threads. SQLite connections
// cannot be shared between threads, so each thread opens its own read-only
// connection the first time it needs one.
#pragma once

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QSqlDatabase>
#include <QString>

class TileStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(QString layout READ layout NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString attribution READ attribution NOTIFY changed)
    Q_PROPERTY(int minZoom READ minZoom NOTIFY changed)
    Q_PROPERTY(int maxZoom READ maxZoom NOTIFY changed)
    Q_PROPERTY(double tileCount READ tileCount NOTIFY changed)

public:
    explicit TileStore(QObject *parent = nullptr);

    // Opens and checks the file. Call once, on the UI thread, before the map is shown.
    bool open(const QString &path);

    bool ready() const { return m_layout != Layout::None; }
    QString error() const { return m_error; }
    QString path() const { return m_path; }
    QString layout() const;
    QString name() const { return m_name; }
    QString attribution() const { return m_attribution; }
    int minZoom() const { return m_minZoom; }
    int maxZoom() const { return m_maxZoom; }
    double tileCount() const { return static_cast<double>(m_tileCount); }

    // Centre of the area the tiles cover (used before the first GPS fix).
    double centerLat() const { return m_centerLat; }
    double centerLon() const { return m_centerLon; }

    // Thread-safe. Returns the tile image for z/x/y (XYZ order, y from the north).
    // If the tile is missing, enlarges part of a lower-zoom tile (overzoom);
    // if nothing covers the area, returns a grey "no tile" image.
    QImage loadTile(int z, int x, int y) const;

Q_SIGNALS:
    void changed();

private:
    enum class Layout { None, MapTilesDb, MBTiles };

    QByteArray readTile(int z, int x, int y) const;   // raw PNG/JPEG bytes, empty if missing
    QSqlDatabase threadConnection() const;            // this thread's read-only connection
    QImage makePlaceholder() const;

    Layout m_layout = Layout::None;
    QString m_path;
    QString m_error;
    QString m_name;
    QString m_attribution;
    int m_minZoom = 0;
    int m_maxZoom = 19;
    qint64 m_tileCount = 0;
    qint64 m_setId = -1;          // maptiles.db: the tile set shown (the default one)
    double m_centerLat = 0.0;
    double m_centerLon = 0.0;
    QImage m_placeholder;
};
