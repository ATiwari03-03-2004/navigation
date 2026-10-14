# gcs_map_app: the GCS laptop's map app (one Qt Creator project)

This project runs on the **GCS laptop**. It is a Qt 6 C++/QML app with its own ROS 2 node.
- The node **subscribes to `/gcs/telemetry`** (`sensor_msgs/msg/NavSatFix`) and reads the latitude and longitude sent by the robot laptop (`robot_map_ws`).
- The app shows that position live on a map.
- The map is drawn from **offline tiles stored in SQLite** (`maps/maptiles.db`); there is no internet at any point.

It's **one CMake project**: open `CMakeLists.txt` in Qt Creator, and it builds all three programs:

| Program | What it does |
|---|---|
| `gcs_map_app` | The GCS map UI and the ROS 2 listener |
| `make_test_mbtiles` | Draws a test offline tile pack: `maps/test_area.mbtiles` |
| `import_mbtiles` | Imports a tile pack into the SQLite tile database, `maps/maptiles.db` |

Steps to install, open in Qt Creator, build and run: [HOW_TO_RUN.md](HOW_TO_RUN.md).

## Contents

```
gcs_map_app/
├── CMakeLists.txt               the only build file: open this in Qt Creator
├── src/                         the app (C++)
│   ├── main.cpp                 wiring: ROS init, models, controllers, QML engine
│   ├── RosBridge.*              ROS 2 listener: rclcpp node + subscription on its own thread
│   ├── TelemetrySample.h        plain C++ copy of one NavSatFix (no ROS types in the UI)
│   ├── TelemetryController.*    latest fix, rate, age, heading from the direction of travel -> QML
│   ├── MapController.*          map view (centre, zoom), visible tiles, robot, trail, follow
│   ├── TileStore.*              reads tiles from SQLite (maptiles.db or .mbtiles)
│   ├── TileImageProvider.*      image://maptiles/z/x/y -> tile image, on worker threads
│   └── GeoMath.h                Web Mercator maths, distance, bearing
├── qml/                         the UI: Main.qml, MapView.qml, RobotMarker.qml, MapButton.qml, InfoRow.qml
├── tools/                       the two tile programs (C++, Qt only)
│   ├── make_test_mbtiles.cpp
│   ├── import_mbtiles.cpp
│   └── TileMath.h
├── maps/                        test_area.mbtiles and maptiles.db are created here
├── README.md
└── HOW_TO_RUN.md
```

## How it works

```
/gcs/telemetry ──▶ RosBridge ──(queued)──▶ TelemetryController ──▶ MapController ◀── QML map (pan, zoom, follow)
 NavSatFix          ROS thread               UI thread                │                    │
 (from robot)                                                         │        image://maptiles/z/x/y
                                                                      ▼                    ▼
                                                             robot + trail       TileImageProvider (4 worker threads)
                                                             (memory only)               │
                                                                                         ▼
                                                                         TileStore ── SELECT tile_data ──▶ maps/maptiles.db (SQLite)
```

1. **ROS 2 listener (`RosBridge`).**
   - The app starts an `rclcpp` node, `gcs_map_app`, and subscribes to `/gcs/telemetry` as `sensor_msgs/msg/NavSatFix`, with best-effort QoS like the robot's publisher.
   - The node spins on its **own thread**, so ROS never freezes the screen.
   - Each message is copied into a plain C++ struct and handed to the UI thread with a queued Qt call.
2. **Telemetry state (`TelemetryController`).**
   - Keeps the latest latitude, longitude, altitude, fix status and accuracy.
   - Measures the message rate, and shows **Live** while the last message is at most 1 s old, **Stale** after that.
   - A position is used only with a GPS fix and valid numbers.
   - NavSatFix has no heading, so the heading is worked out from the **direction of travel**, once the robot has moved at least 2 m, so GPS noise doesn't spin the arrow.
3. **The map logic (`MapController`).**
   - Converts latitude/longitude to Web Mercator pixels: `x = (lon+180)/360·256·2^z`, `y = (1 − ln(tan φ + sec φ)/π)/2·256·2^z`.
   - Works out which 256 px tiles cover the view, and places the robot arrow.
   - Keeps a **trail**: a point every metre, in memory only.
   - **Follows** the robot until you drag the map; the ◎ button follows it again.
4. **Offline tiles in SQLite (`TileStore`, `TileImageProvider`).**
   - QML asks for `image://maptiles/z/x/y`. A worker thread runs one indexed SQLite query, using its own read-only Qt SQL connection, and decodes the PNG.
   - Missing zoom levels are filled by enlarging a lower-zoom tile; areas with no tiles show a grey grid.
   - `maptiles.db` uses the HLD's tables (`tile_set`, `tile`, `set_tile`). The app can also open a raw `.mbtiles` file directly.
5. **Tile programs (`tools/`).**
   - `make_test_mbtiles` draws a test map with no internet: a labelled latitude/longitude grid, distance rings and a START dot around a point.
   - `import_mbtiles` imports any raster MBTiles pack into `maptiles.db`.
   - By default both use this project's `maps/` folder, and the app reads `maps/maptiles.db`, so no run arguments are needed in Qt Creator.
   - For a real map, make an MBTiles pack in QGIS from OpenStreetMap data, or from drone imagery, and import it with `import_mbtiles my_area.mbtiles <project>/maps/maptiles.db --default`.

What you see:
- A top bar with the link status (**Live · 5 Hz**).
- The map with the robot arrow and its trail, **+ / − / ◎** buttons, a scale bar and the tile attribution.
- A side panel with the GPS fix, map and tile-database details.
