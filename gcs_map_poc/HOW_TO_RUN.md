# How to run the GCS map POC

Target: **Ubuntu 24.04, ROS 2 Jazzy, Qt 6 (6.4 or newer)**. The steps below assume the folder is at `~/gcs_map_poc`. Change the path if you put it somewhere else.

What you will have at the end:

- **Terminal 1:** a fake robot (`gps_sim`) publishing a GPS position on `/gcs/telemetry` that moves a little every 0.1 s.
- **Terminal 2:** the Qt GCS map app (`gcs_map_app`), showing that position live on a map drawn from offline tiles in a local SQLite file.

---

## 1. Install (once)

```bash
# ROS 2 Jazzy + build tools (skip the first line if ROS 2 Jazzy is already installed)
sudo apt install ros-jazzy-ros-base
sudo apt install ros-dev-tools

# Qt 6: C++ libraries, QML, and the SQLite driver used by Qt SQL
sudo apt install qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite \
  qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-shapes \
  qml6-module-qtqml-workerscript qt6-qpa-plugins cmake g++

# Qt Creator (optional; or use the one from the Qt online installer)
sudo apt install qtcreator

# Optional: the sqlite3 command line, handy for looking inside the tile databases
sudo apt install sqlite3
```

If ROS 2 Jazzy is not installed yet, follow the official guide first to add the ROS apt repository: <https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html>.

---

## 2. Build the ROS 2 workspace

The repository is a colcon workspace. Its three packages are in `src/`.

```bash
cd ~/gcs_map_poc
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install
source install/setup.bash
```

Expected result: `Summary: 4 packages finished` (`gcs_interfaces`, `gps_sim`, `gcs_tile_tools`, `gcs_map_app`). All code is C++.

Check that the message type was generated:

```bash
ros2 interface show gcs_interfaces/msg/Telemetry
```

---

## 3. Make the offline map tiles (once)

No internet is needed. The first command draws a synthetic tile pack: a labelled latitude/longitude grid, distance rings and a START marker around the start point. The second imports the pack into the GCS tile database `maptiles.db`, which is a SQLite file.

```bash
cd ~/gcs_map_poc
ros2 run gcs_tile_tools make_test_mbtiles --out maps/test_area.mbtiles
ros2 run gcs_tile_tools import_mbtiles maps/test_area.mbtiles maps/maptiles.db
ros2 run gcs_tile_tools import_mbtiles --list maps/maptiles.db
```

The defaults cover a 4 km × 4 km area around 28.6129 N, 77.2295 E at zoom 12 to 18: about 1,300 tiles and 5 MB, made in a few seconds.

To use another place, change the centre. Use the same start point for the simulator in step 4.

```bash
ros2 run gcs_tile_tools make_test_mbtiles --lat 12.9716 --lon 77.5946 --radius-km 3 --out maps/blr.mbtiles
ros2 run gcs_tile_tools import_mbtiles maps/blr.mbtiles maps/maptiles.db --default
```

To use real map tiles instead, see "Using real map tiles" in `README.md`. The app can also open an `.mbtiles` file directly, with `--tiles maps/test_area.mbtiles`.

Optional: make the database the app's default, so `--tiles` is not needed. This is the path from the HLD.

```bash
mkdir -p ~/.local/share/gcs
cp maps/maptiles.db ~/.local/share/gcs/maptiles.db
```

---

## 4. Run

Every new terminal needs both `source` lines:

```bash
source /opt/ros/jazzy/setup.bash
source ~/gcs_map_poc/install/setup.bash
```

**Terminal 1: the fake robot**

```bash
ros2 run gps_sim gps_sim
```

It publishes about 10 messages a second and logs every 5 s:

```
[INFO] [gps_sim]: Publishing QR-01 on /gcs/telemetry at 10.0 Hz, start 28.612900, 77.229500, pattern 'line'
[INFO] [gps_sim]: seq 50: 28.613000, 77.229650, heading 53.0 deg
```

**Terminal 2: the GCS map app**

```bash
ros2 run gcs_map_app gcs_map_app --tiles ~/gcs_map_poc/maps/maptiles.db
```

What you should see:

- The top bar reads **Live · 10 Hz**.
- The map centres on the robot: a blue arrow pointing north-east, with a blue trail behind it. It starts at the red START dot.
- The side panel shows the robot ID, sequence number, latitude and longitude changing.
- Grid lines and labels on the tiles let you check the position: the marker sits where the side panel's latitude and longitude meet on the grid.

Using the map:

| Action | How |
|---|---|
| Pan | Drag the map. This turns off "follow robot". |
| Zoom | Mouse wheel, double-click, or the **+** and **−** buttons. You can zoom 2 levels past the deepest tiles; those tiles are enlarged. |
| Follow the robot again | **◎** button |
| Clear the trail | **Clear trail** in the side panel |

Stop the simulator with Ctrl+C. Within a second, the badge turns **STALE** and the marker turns grey. Start it again and the badge returns to **LIVE**.

### Options

Simulator parameters (`--ros-args -p name:=value`):

| Parameter | Default | Meaning |
|---|---|---|
| `start_lat`, `start_lon` | 28.6129, 77.2295 | Start position (degrees) |
| `lat_step`, `lon_step` | 0.000002, 0.000003 | Added to latitude and longitude on every message (about 0.2 m and 0.3 m) |
| `rate_hz` | 10.0 | Messages per second |
| `pattern` | `line` | `line`: keep adding the steps. `square`: walk a square and repeat. |
| `square_side_steps` | 300 | Steps per side of the square |
| `max_steps` | 3000 | After this many steps, jump back to the start, so the robot stays on the tiles (0 = never) |
| `robot_id` | `QR-01` | Sent in every message |
| `drop_fix_every` | 0 | Every N messages, send one without a GPS fix, to test the "NO GPS FIX" badge |
| `topic` | `/gcs/telemetry` | Topic to publish on |

Examples:

```bash
ros2 run gps_sim gps_sim --ros-args -p pattern:=square -p square_side_steps:=200
ros2 run gps_sim gps_sim --ros-args -p lat_step:=0.00001 -p lon_step:=0.0 -p rate_hz:=5.0
ros2 run gps_sim gps_sim --ros-args -p start_lat:=12.9716 -p start_lon:=77.5946
```

App options:

| Option | Default | Meaning |
|---|---|---|
| `--tiles <file>` | `$GCS_MAPTILES`, else `~/.local/share/gcs/maptiles.db` | `maptiles.db` or an `.mbtiles` pack |
| `--zoom <z>` | 17 | Start zoom |
| `--ros-args -p telemetry_topic:=<topic>` | `/gcs/telemetry` | Topic to subscribe to |

---

## 5. Check the topic from the command line

```bash
ros2 topic list                                   # /gcs/telemetry should be listed
ros2 topic hz /gcs/telemetry                      # about 10 Hz
ros2 topic echo /gcs/telemetry --qos-profile sensor_data --once
```

`--qos-profile sensor_data` matches the publisher's QoS (best effort).

You can also publish a single position by hand, with the simulator stopped:

```bash
ros2 topic pub --once -w 1 /gcs/telemetry gcs_interfaces/msg/Telemetry \
  "{robot_id: 'QR-01', seq: 1, gps: {latitude: 28.6135, longitude: 77.2300, status: {status: 0}}, heading_deg: 90.0}"
```

---

## 6. Working in Qt Creator

Qt Creator must inherit the ROS environment, so start it from a terminal where both setup files are sourced.

1. Build the message package once with colcon, so Qt Creator can find it:

   ```bash
   cd ~/gcs_map_poc
   source /opt/ros/jazzy/setup.bash
   colcon build --packages-select gcs_interfaces gps_sim gcs_tile_tools
   source install/setup.bash
   qtcreator &
   ```

2. **File > Open File or Project**, then choose `~/gcs_map_poc/src/gcs_map_app/CMakeLists.txt`.
3. Pick a **Desktop Qt 6** kit. The system Qt from apt is `/usr/lib/qt6/bin/qmake6`. A kit from the Qt online installer also works, if it is 6.4 or newer.
4. **Projects > Run > Command line arguments:** `--tiles /home/<you>/gcs_map_poc/maps/maptiles.db`
5. Build and run (Ctrl+R). Start the simulator in a terminal (step 4, Terminal 1).

The QML files are compiled into the binary as Qt resources, so rebuild after you edit them. If you change `Telemetry.msg`, rebuild `gcs_interfaces` with colcon, then re-run CMake in Qt Creator (**Build > Run CMake**).

---

## 7. Two machines (robot and GCS)

Run `gps_sim` on one machine and `gcs_map_app` on another, on the same network:

- Both need the same `ROS_DOMAIN_ID`, for example `export ROS_DOMAIN_ID=21`.
- Both need the `gcs_interfaces` package built.
- If multicast discovery is blocked (common on radios and Wi-Fi), point each machine at the other:

  ```bash
  export ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST
  export ROS_STATIC_PEERS=<other machine's IP>
  ```

  This is the setup the HLD describes (Section 5.2).

---

## 8. Troubleshooting

| Problem | Fix |
|---|---|
| `Could not find a package configuration file provided by "gcs_interfaces"` | Source the workspace (`source install/setup.bash`) before building `gcs_map_app`, or build all packages with colcon. In Qt Creator, start it from a sourced terminal. |
| `Could not find a package configuration file provided by "Qt6"` | Run `sudo apt install qt6-base-dev qt6-declarative-dev`, or set `CMAKE_PREFIX_PATH` to your Qt install, for example `~/Qt/6.8.0/gcc_64`. |
| `module "QtQuick.Shapes" is not installed` (or `QtQuick.Window`) | Run `sudo apt install qml6-module-qtquick-shapes qml6-module-qtquick-window qml6-module-qtquick`. |
| `QSqlDatabase: QSQLITE driver not loaded` | Run `sudo apt install libqt6sql6-sqlite`. |
| Map shows "Offline map tiles not loaded" | The `--tiles` path is wrong, or the file is missing. Use an absolute path. Run `ros2 run gcs_tile_tools import_mbtiles --list <file>`. |
| Grey tiles saying "No map tiles here" | The robot is outside the tile pack. Make a pack around the simulator's start point (same `--lat` and `--lon`), or lower `max_steps`. |
| Top bar stays on "Waiting for /gcs/telemetry" | Is `gps_sim` running? Run `ros2 topic hz /gcs/telemetry`. Both terminals need the same `ROS_DOMAIN_ID`, and both must have sourced `install/setup.bash`. |
| `ros2 topic echo` prints nothing | Add `--qos-profile sensor_data`. The publisher is best effort. |
| `Could not find the Qt platform plugin "offscreen"` (tile tools) | Run `sudo apt install qt6-qpa-plugins`. |
| `qt.qpa.xcb: could not connect to display` (SSH or WSL) | The app needs a desktop session. Run it on the Ubuntu desktop, or use WSLg or X forwarding. |
| Wayland warnings | Harmless. To force X11: `export QT_QPA_PLATFORM=xcb`. |
