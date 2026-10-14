// GCS map POC: subscribes to /gcs/telemetry (ROS 2 Jazzy) and shows the
// robot's GPS position live on a map drawn from offline tiles in SQLite.
//
// Usage:
//   gcs_map_app [--tiles <maptiles.db | pack.mbtiles>] [--zoom <z>]
//               [--ros-args -p telemetry_topic:=/gcs/telemetry]
//
// Tile file lookup order: --tiles, $GCS_MAPTILES, <project>/maps/maptiles.db (GCS_MAPS_DIR)
#include <algorithm>

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>

#include <rclcpp/rclcpp.hpp>

#include "MapController.h"
#include "RosBridge.h"
#include "TelemetryController.h"
#include "TileImageProvider.h"
#include "TileStore.h"

namespace {

QString defaultTilesPath()
{
    const QString env = qEnvironmentVariable("GCS_MAPTILES");
    if (!env.isEmpty())
        return env;
#ifdef GCS_MAPS_DIR
    return QStringLiteral(GCS_MAPS_DIR "/maptiles.db");   // set by CMakeLists.txt: <project>/maps
#else
    return QDir::homePath() + QStringLiteral("/.local/share/gcs/maptiles.db");
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    // ROS first: it reads (and removes from its view) the --ros-args part.
    rclcpp::init(argc, argv);
    const std::vector<std::string> nonRosArgs = rclcpp::remove_ros_arguments(argc, argv);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("gcs_map_app"));
    QGuiApplication::setOrganizationName(QStringLiteral("RoboGCS"));

    // App options, parsed from the non-ROS arguments only.
    QStringList appArgs;
    for (const std::string &a : nonRosArgs)
        appArgs << QString::fromStdString(a);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("GCS map POC: live robot position on offline map tiles"));
    parser.addHelpOption();
    QCommandLineOption tilesOpt(QStringLiteral("tiles"),
                                QStringLiteral("Tile database: maptiles.db or an .mbtiles pack."),
                                QStringLiteral("path"), defaultTilesPath());
    QCommandLineOption zoomOpt(QStringLiteral("zoom"), QStringLiteral("Start zoom level."),
                               QStringLiteral("z"), QStringLiteral("17"));
    parser.addOption(tilesOpt);
    parser.addOption(zoomOpt);
    if (!parser.parse(appArgs))
        qWarning("%s", qPrintable(parser.errorText()));
    if (parser.isSet(QStringLiteral("help"))) {
        parser.showHelp(0);
    }

    // ---- Models
    TileStore tileStore;   // Map Tile Model (offline tiles in SQLite)
    if (!tileStore.open(parser.value(tilesOpt)))
        qWarning("Map tiles: %s", qPrintable(tileStore.error()));
    else
        qInfo("Map tiles: %s, %s, zoom %d-%d, %.0f tiles",
              qPrintable(tileStore.path()), qPrintable(tileStore.layout()),
              tileStore.minZoom(), tileStore.maxZoom(), tileStore.tileCount());

    // ---- Controllers
    // Allow zooming 2 levels past the deepest tiles (overzoom enlarges them).
    const int minZoom = tileStore.ready() ? std::max(0, tileStore.minZoom()) : 2;
    const int maxZoom = tileStore.ready() ? std::min(21, tileStore.maxZoom() + 2) : 19;
    MapController mapController(minZoom, maxZoom, parser.value(zoomOpt).toInt(),
                                tileStore.centerLat(), tileStore.centerLon());

    TelemetryController telemetry;
    RosBridge rosBridge(&telemetry);   // ROS node + subscription; posts samples to telemetry

    QObject::connect(&telemetry, &TelemetryController::positionUpdated,
                     &mapController, &MapController::onPosition);

    // ---- Views (QML)
    // The engine is declared after the models and controllers, so it is
    // destroyed first: its image provider stops using tileStore before
    // tileStore goes away.
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("maptiles"), new TileImageProvider(&tileStore));
    engine.rootContext()->setContextProperty(QStringLiteral("tileStore"), &tileStore);
    engine.rootContext()->setContextProperty(QStringLiteral("mapController"), &mapController);
    engine.rootContext()->setContextProperty(QStringLiteral("telemetry"), &telemetry);
    // The topic actually used (it can be changed with -p telemetry_topic:=...)
    engine.rootContext()->setContextProperty(QStringLiteral("telemetryTopic"),
                                             QString::fromStdString(rosBridge.topic()));
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        rclcpp::shutdown();
        return 1;
    }

    rosBridge.start();
    const int rc = app.exec();

    rosBridge.stop();
    if (rclcpp::ok())
        rclcpp::shutdown();
    return rc;
}
