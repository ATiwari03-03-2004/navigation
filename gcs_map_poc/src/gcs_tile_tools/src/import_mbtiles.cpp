// import_mbtiles: imports an MBTiles tile pack into the GCS map tile database (maptiles.db).
//
// This is the "Import a tile pack" step of the HLD (Section 5.5, Figure 5.5-1),
// done as a command-line tool for the POC. maptiles.db uses the HLD schema
// (Section 4.2.3):
//   tile_set  one row per imported pack (name, source, attribution, area, zoom levels, ...)
//   tile      one row per tile image, addressed by (source, zoom, x, y) in XYZ order
//   set_tile  which tiles belong to which tile set (a shared tile is stored once)
//
// MBTiles counts tile rows from the south (TMS); maptiles.db stores XYZ rows
// (from the north), so every row is converted: y = 2^z - 1 - tile_row.
//
// Usage:
//   import_mbtiles PACK.mbtiles maptiles.db             import (the first set becomes the default)
//   import_mbtiles PACK.mbtiles maptiles.db --default   import and make it the default
//   import_mbtiles --list maptiles.db                   list tile sets
//   import_mbtiles --remove "POC test area" maptiles.db remove a tile set

#include <cstdio>
#include <cstdlib>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

#include "TileMath.h"

namespace {

const QStringList kSchema = {
    "PRAGMA foreign_keys = ON",
    "CREATE TABLE IF NOT EXISTS tile_set ("
    "  set_id       INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name         TEXT    NOT NULL UNIQUE,"
    "  source       TEXT    NOT NULL,"
    "  attribution  TEXT,"
    "  north REAL, south REAL, east REAL, west REAL,"
    "  min_zoom     INTEGER,"
    "  max_zoom     INTEGER,"
    "  tile_count   INTEGER NOT NULL DEFAULT 0,"
    "  size_bytes   INTEGER NOT NULL DEFAULT 0,"
    "  is_default   INTEGER NOT NULL DEFAULT 0,"
    "  imported_by  INTEGER,"            // user_id in gcs.db (logical link; NULL in the POC)
    "  imported_at  INTEGER NOT NULL)",  // Unix time, seconds
    "CREATE TABLE IF NOT EXISTS tile ("
    "  tile_id     INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  source      TEXT    NOT NULL,"
    "  zoom        INTEGER NOT NULL,"
    "  x           INTEGER NOT NULL,"    // column from the west
    "  y           INTEGER NOT NULL,"    // row from the north (XYZ)
    "  format      TEXT    NOT NULL,"    // png or jpg
    "  tile_data   BLOB    NOT NULL,"
    "  size_bytes  INTEGER NOT NULL,"
    "  added_at    INTEGER NOT NULL,"
    "  UNIQUE (source, zoom, x, y))",
    "CREATE INDEX IF NOT EXISTS idx_tile_zxy ON tile (zoom, x, y)",
    "CREATE TABLE IF NOT EXISTS set_tile ("
    "  set_id   INTEGER NOT NULL REFERENCES tile_set (set_id) ON DELETE CASCADE,"
    "  tile_id  INTEGER NOT NULL REFERENCES tile (tile_id),"
    "  PRIMARY KEY (set_id, tile_id))",
};

[[noreturn]] void fail(const QString &msg)
{
    std::fprintf(stderr, "%s\n", qPrintable(msg));
    std::exit(1);
}

QSqlDatabase openDb(const QString &connection, const QString &path, bool readOnly)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connection);
    db.setDatabaseName(path);
    if (readOnly)
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
    if (!db.open())
        fail(QStringLiteral("Cannot open %1: %2").arg(path, db.lastError().text()));
    return db;
}

void run(QSqlQuery &q, const QString &sql)
{
    if (!q.exec(sql))
        fail(QStringLiteral("SQL error: %1\n  in: %2").arg(q.lastError().text(), sql));
}

void createSchema(QSqlDatabase &db)
{
    QSqlQuery q(db);
    for (const QString &sql : kSchema)
        run(q, sql);
}

// --------------------------------------------------------------------- import
void importPack(const QString &packPath, const QString &dbPath, bool makeDefault, const QString &sourceOverride)
{
    if (!QFileInfo::exists(packPath))
        fail("Pack not found: " + packPath);

    QSqlDatabase pack = openDb("pack", packPath, true);
    const QStringList packTables = pack.tables();
    if (!packTables.contains("tiles") || !packTables.contains("metadata"))
        fail("Not an MBTiles file: it needs the 'metadata' and 'tiles' tables");

    QHash<QString, QString> meta;
    QSqlQuery pq(pack);
    run(pq, "SELECT name, value FROM metadata");
    while (pq.next())
        meta.insert(pq.value(0).toString(), pq.value(1).toString());

    // Checks before importing (HLD Figure 5.5-1: "Valid MBTiles file?")
    QString format = meta.value("format", "png").toLower();
    if (format == "jpeg")
        format = "jpg";
    if (format != "png" && format != "jpg")
        fail("Unsupported tile format '" + format + "': only raster png or jpg packs can be shown");

    const QString name = meta.value("name").isEmpty() ? QFileInfo(packPath).completeBaseName() : meta.value("name");
    const QString source = sourceOverride.isEmpty() ? name : sourceOverride;
    const QString attribution = meta.value("attribution");
    QVariant west, south, east, north;   // NULL if unknown
    const QStringList b = meta.value("bounds").split(',');
    if (b.size() == 4) {
        west = b[0].toDouble(); south = b[1].toDouble(); east = b[2].toDouble(); north = b[3].toDouble();
    }

    run(pq, "SELECT MIN(zoom_level), MAX(zoom_level), COUNT(*) FROM tiles");
    pq.next();
    if (pq.value(2).toLongLong() == 0)
        fail("The pack has no tiles");
    const int minZoom = meta.contains("minzoom") ? meta.value("minzoom").toInt() : pq.value(0).toInt();
    const int maxZoom = meta.contains("maxzoom") ? meta.value("maxzoom").toInt() : pq.value(1).toInt();

    QSqlDatabase db = openDb("db", dbPath, false);
    createSchema(db);
    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM tile_set WHERE name = ?");
    q.addBindValue(name);
    q.exec();
    if (q.next())
        fail(QStringLiteral("A tile set named '%1' already exists in %2. Remove it first with --remove.").arg(name, dbPath));

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QElapsedTimer timer;
    timer.start();

    // One transaction: if anything fails, nothing changes.
    if (!db.transaction())
        fail("Cannot start a transaction: " + db.lastError().text());

    auto rollbackAndFail = [&db](const QString &why) {
        db.rollback();
        fail(why);
    };

    run(q, "SELECT COUNT(*) FROM tile_set");
    q.next();
    const bool firstSet = q.value(0).toLongLong() == 0;

    q.prepare("INSERT INTO tile_set (name, source, attribution, north, south, east, west, min_zoom, max_zoom, "
              "is_default, imported_by, imported_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0, NULL, ?)");
    for (const QVariant &v : {QVariant(name), QVariant(source), QVariant(attribution), north, south, east, west,
                              QVariant(minZoom), QVariant(maxZoom), QVariant(now)})
        q.addBindValue(v);
    if (!q.exec())
        rollbackAndFail("Cannot add the tile set: " + q.lastError().text());
    const qint64 setId = q.lastInsertId().toLongLong();

    QSqlQuery insTile(db), findTile(db), link(db);
    insTile.prepare("INSERT OR IGNORE INTO tile (source, zoom, x, y, format, tile_data, size_bytes, added_at) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    findTile.prepare("SELECT tile_id FROM tile WHERE source = ? AND zoom = ? AND x = ? AND y = ?");
    link.prepare("INSERT OR IGNORE INTO set_tile (set_id, tile_id) VALUES (?, ?)");

    qint64 added = 0, linked = 0, size = 0;
    QSqlQuery tiles(pack);
    tiles.setForwardOnly(true);
    run(tiles, "SELECT zoom_level, tile_column, tile_row, tile_data FROM tiles");
    while (tiles.next()) {
        const int z = tiles.value(0).toInt();
        const int x = tiles.value(1).toInt();
        const int y = tiles::flipRow(z, tiles.value(2).toInt());   // TMS row -> XYZ row
        const QByteArray data = tiles.value(3).toByteArray();

        insTile.addBindValue(source);
        insTile.addBindValue(z);
        insTile.addBindValue(x);
        insTile.addBindValue(y);
        insTile.addBindValue(format);
        insTile.addBindValue(data);
        insTile.addBindValue(data.size());
        insTile.addBindValue(now);
        if (!insTile.exec())
            rollbackAndFail("Cannot add a tile: " + insTile.lastError().text());

        qint64 tileId;
        if (insTile.numRowsAffected() == 1) {
            tileId = insTile.lastInsertId().toLongLong();
            ++added;
        } else {
            // The same tile is already stored for another set: link it instead of copying.
            findTile.addBindValue(source);
            findTile.addBindValue(z);
            findTile.addBindValue(x);
            findTile.addBindValue(y);
            if (!findTile.exec() || !findTile.next())
                rollbackAndFail("Cannot find an existing tile: " + findTile.lastError().text());
            tileId = findTile.value(0).toLongLong();
            findTile.finish();
        }

        link.addBindValue(setId);
        link.addBindValue(tileId);
        if (!link.exec())
            rollbackAndFail("Cannot link a tile: " + link.lastError().text());
        ++linked;
        size += data.size();
    }

    q.prepare("UPDATE tile_set SET tile_count = ?, size_bytes = ? WHERE set_id = ?");
    q.addBindValue(linked);
    q.addBindValue(size);
    q.addBindValue(setId);
    if (!q.exec())
        rollbackAndFail(q.lastError().text());

    const bool isDefault = makeDefault || firstSet;
    if (isDefault) {
        q.prepare("UPDATE tile_set SET is_default = CASE WHEN set_id = ? THEN 1 ELSE 0 END");
        q.addBindValue(setId);
        if (!q.exec())
            rollbackAndFail(q.lastError().text());
    }
    if (!db.commit())
        rollbackAndFail("Commit failed: " + db.lastError().text());

    std::printf("Imported '%s' into %s: %lld tiles (%lld new, %lld shared), %.1f MB, zoom %d-%d, %.1f s%s\n",
                qPrintable(name), qPrintable(dbPath), linked, added, linked - added, size / 1e6,
                minZoom, maxZoom, timer.elapsed() / 1000.0, isDefault ? "  [default]" : "");
}

// --------------------------------------------------------------------- list / remove
void listSets(const QString &dbPath)
{
    QSqlDatabase db = openDb("db", dbPath, true);
    if (!db.tables().contains("tile_set")) {
        std::printf("No tile sets.\n");
        return;
    }
    QSqlQuery q(db);
    run(q, "SELECT set_id, name, min_zoom, max_zoom, tile_count, size_bytes, is_default FROM tile_set ORDER BY set_id");
    bool any = false;
    while (q.next()) {
        any = true;
        std::printf("%3lld  %-30s zoom %d-%d  %7lld tiles  %7.1f MB%s\n",
                    q.value(0).toLongLong(), qPrintable(q.value(1).toString()), q.value(2).toInt(),
                    q.value(3).toInt(), q.value(4).toLongLong(), q.value(5).toLongLong() / 1e6,
                    q.value(6).toInt() ? "  [default]" : "");
    }
    if (!any)
        std::printf("No tile sets.\n");
}

void removeSet(const QString &name, const QString &dbPath)
{
    QSqlDatabase db = openDb("db", dbPath, false);
    QSqlQuery q(db);
    run(q, "PRAGMA foreign_keys = ON");
    q.prepare("SELECT set_id, is_default FROM tile_set WHERE name = ?");
    q.addBindValue(name);
    if (!q.exec() || !q.next())
        fail("No tile set named '" + name + "'");
    const qint64 setId = q.value(0).toLongLong();
    const bool wasDefault = q.value(1).toInt() == 1;
    q.finish();

    db.transaction();
    q.prepare("DELETE FROM set_tile WHERE set_id = ?");
    q.addBindValue(setId);
    q.exec();
    q.prepare("DELETE FROM tile_set WHERE set_id = ?");
    q.addBindValue(setId);
    q.exec();
    // Then every tile that no longer belongs to any set (HLD 4.2.3).
    run(q, "DELETE FROM tile WHERE tile_id NOT IN (SELECT tile_id FROM set_tile)");
    const int gone = q.numRowsAffected();
    if (wasDefault)
        run(q, "UPDATE tile_set SET is_default = 1 WHERE set_id = (SELECT MIN(set_id) FROM tile_set)");
    if (!db.commit())
        fail("Commit failed: " + db.lastError().text());
    run(q, "VACUUM");
    std::printf("Removed '%s' and %d tiles no longer used by any set.\n", qPrintable(name), gone);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Import an MBTiles tile pack into the GCS map tile database (maptiles.db).");
    parser.addHelpOption();
    parser.addPositionalArgument("pack", "MBTiles file to import (not needed with --list or --remove).", "[pack]");
    parser.addPositionalArgument("db", "maptiles.db (created if missing).");
    QCommandLineOption defaultOpt("default", "Make the imported set the default.");
    QCommandLineOption sourceOpt("source", "Source name stored with each tile (default: the pack's name).", "name");
    QCommandLineOption listOpt("list", "List the tile sets in the database.");
    QCommandLineOption removeOpt("remove", "Remove a tile set by name.", "name");
    parser.addOptions({defaultOpt, sourceOpt, listOpt, removeOpt});
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (parser.isSet(listOpt) || parser.isSet(removeOpt)) {
        if (args.size() != 1)
            parser.showHelp(1);
        if (parser.isSet(listOpt))
            listSets(args[0]);
        else
            removeSet(parser.value(removeOpt), args[0]);
    } else {
        if (args.size() != 2)
            parser.showHelp(1);
        importPack(args[0], args[1], parser.isSet(defaultOpt), parser.value(sourceOpt));
    }
    return 0;
}
