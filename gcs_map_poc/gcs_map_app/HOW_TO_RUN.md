# gcs_map_app: how to build and run (GCS laptop, Qt Creator)

Target: **Ubuntu 24.04 + ROS 2 Jazzy + Qt 6 + Qt Creator**. The paths assume the project is at `~/gcs_map_app`.

This is a **single Qt Creator project**, with no colcon and no extra packages to build first. The order of work:
1. Install the tools once.
2. Set up the network.
3. Start Qt Creator from a terminal where ROS is sourced.
4. Open `CMakeLists.txt` and configure.
5. Run `make_test_mbtiles`, then `import_mbtiles`, then `gcs_map_app`.

---

## 1. Install (once)

**ROS 2 Jazzy** (skip if installed). These are the official steps from docs.ros.org:

```bash
sudo apt install software-properties-common curl
sudo add-apt-repository universe
export ROS_APT_SOURCE_VERSION=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest | grep -F "tag_name" | awk -F\" '{print $4}')
curl -L -o /tmp/ros2-apt-source.deb "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo $VERSION_CODENAME)_all.deb"
sudo dpkg -i /tmp/ros2-apt-source.deb
sudo apt update
sudo apt install ros-jazzy-ros-base
```

If these commands have changed, follow <https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html>. `ros-jazzy-ros-base` includes `rclcpp` and `sensor_msgs`, which is all the app needs from ROS.

**Qt 6, its QML modules, the SQLite driver, and Qt Creator:**

```bash
sudo apt install cmake g++ \
  qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite qt6-qpa-plugins \
  qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-shapes \
  qml6-module-qtqml-workerscript \
  qtcreator
```

| Package | Why |
|---|---|
| `qt6-base-dev`, `qt6-declarative-dev` | Qt 6 C++ and QML (Ubuntu 24.04 ships Qt 6.4.2) |
| `libqt6sql6-sqlite` | Qt SQL's SQLite driver (`QSQLITE`): reads `maptiles.db` |
| `qt6-qpa-plugins` | Lets `make_test_mbtiles` draw tiles without opening a window |
| `qml6-module-qtquick-shapes` | Draws the trail and the robot arrow |
| `qtcreator` | The IDE. The Qt online installer's Qt Creator also works. |

---

## 2. Get the project

Copy the `gcs_map_app` folder to the GCS laptop as `~/gcs_map_app`, or clone the repository and use that folder.

---

## 3. The build file, explained (`CMakeLists.txt`)

There's one build file and no `package.xml`; Qt Creator only needs `CMakeLists.txt`. This is what it does, section by section:

```cmake
cmake_minimum_required(VERSION 3.16)
project(gcs_map_app LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# ROS 2 Jazzy from /opt/ros/jazzy (the hint also works if Qt Creator was not started from a sourced terminal)
list(APPEND CMAKE_PREFIX_PATH "/opt/ros/jazzy")
find_package(ament_cmake REQUIRED)     # ROS 2 CMake helpers
find_package(rclcpp REQUIRED)          # ROS 2 C++: the subscriber
find_package(sensor_msgs REQUIRED)     # the NavSatFix message

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Sql)   # Sql = SQLite tiles
qt_standard_project_setup()            # AUTOMOC for Q_OBJECT classes

set(GCS_MAPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/maps")   # default tile folder, compiled into the programs

# 1) the app
qt_add_executable(gcs_map_app src/main.cpp src/RosBridge.cpp ... src/TileImageProvider.cpp)
qt_add_resources(gcs_map_app "gcs_map_qml" PREFIX "/" FILES qml/Main.qml qml/MapView.qml ...)  # QML inside the binary
target_compile_definitions(gcs_map_app PRIVATE QT_NO_KEYWORDS GCS_MAPS_DIR="${GCS_MAPS_DIR}")
target_link_libraries(gcs_map_app PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Sql
                      rclcpp::rclcpp ${sensor_msgs_TARGETS})

# 2) and 3) the tile programs (Qt only, no ROS)
qt_add_executable(make_test_mbtiles tools/make_test_mbtiles.cpp tools/TileMath.h)
target_link_libraries(make_test_mbtiles PRIVATE Qt6::Core Qt6::Gui Qt6::Sql)
qt_add_executable(import_mbtiles tools/import_mbtiles.cpp tools/TileMath.h)
target_link_libraries(import_mbtiles PRIVATE Qt6::Core Qt6::Sql)
```

Notes:
- **No custom message package.** The robot publishes the standard `sensor_msgs/msg/NavSatFix`, which comes with ROS 2. That's why everything fits in one project, with no `colcon` step and no interface folder.
- **`QT_NO_KEYWORDS`:** the code uses `Q_SIGNALS` / `Q_EMIT`, so Qt's `signals`/`emit` macros can't clash with other headers.
- **QML is compiled into the binary.** Rebuild (Ctrl+B) after editing a `.qml` file.
- **Default tile folder.** `GCS_MAPS_DIR` makes all three programs use `<project>/maps` by default, so no run arguments are needed.

---

## 4. Network setup (both laptops)

Do the same on the robot laptop. Put the `export` lines in `~/.bashrc`.

1. Both laptops on the same Wi-Fi or LAN. Check with `hostname -I` on each, and `ping <robot laptop IP>`.
2. The same domain ID on both: `export ROS_DOMAIN_ID=21`.
3. Test discovery: run `ros2 multicast receive` here and `ros2 multicast send` on the robot laptop. If nothing arrives, use static peers:
   - here: `export ROS_STATIC_PEERS=<robot laptop IP>`
   - on the robot: `export ROS_STATIC_PEERS=<this GCS laptop IP>`
4. If the firewall is on: `sudo ufw allow from <robot laptop IP>`.
5. With `gps_sim` running on the robot laptop, check from here:

   ```bash
   source /opt/ros/jazzy/setup.bash
   ros2 topic hz /gcs/telemetry                                     # about 5 Hz
   ros2 topic echo /gcs/telemetry --qos-profile sensor_data --once  # a NavSatFix with latitude/longitude
   ```

   If this works, the app will receive the positions too.

---

## 5. Open, build and run in Qt Creator

### 5.1 Start Qt Creator from a sourced terminal

The app needs the ROS environment at run time to find its DDS libraries. A Qt Creator started from this terminal passes the environment on to the programs it runs.

```bash
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=21                         # same as the robot laptop
# export ROS_STATIC_PEERS=<robot laptop IP>     # only if multicast didn't work (step 4)
qtcreator &
```

Starting Qt Creator from the app menu instead? See 5.7.

### 5.2 Open the project

**File > Open File or Project…**, choose **`~/gcs_map_app/CMakeLists.txt`**, then **Open**.

### 5.3 Configure

1. On the **Configure Project** page, tick a **Desktop kit with Qt 6**, for example "Desktop Qt 6.4.2 GCC 64bit". The name varies; check that the Qt version is 6.x.
2. If there's no Qt 6 kit:
   - **Edit > Preferences > Kits > Qt Versions > Add…**, then choose `/usr/lib/qt6/bin/qmake6`.
   - In **Kits**, select the Desktop kit and set its **Qt version** to that Qt 6.
3. Click **Configure Project**.
4. The **General Messages** pane should end with `-- Configuring done` and `-- Generating done`.

### 5.4 Build

**Build > Build Project "gcs_map_app"** (Ctrl+B). It builds all three programs. Expected: `Build finished` with no errors.

### 5.5 Make the offline map tiles (once)

Qt Creator's **run-target selector** is the monitor icon above the green Run button, bottom left. It lists the three programs. Run them in this order:

1. Select **`make_test_mbtiles`** and press **Run** (Ctrl+R). In **Application Output** you should see:

   ```
     zoom 12:     2 tiles ...
     ...
     zoom 18:   961 tiles ...
   Wrote 1313 tiles, x.x MB, to /home/<you>/gcs_map_app/maps/test_area.mbtiles in x.x s
   ```

   This is a 4 km × 4 km test map around 28.6129 N, 77.2295 E, zoom 12–18: a labelled lat/long grid, distance rings and a START dot.

2. Select **`import_mbtiles`** and press **Run**:

   ```
   Imported 'POC test area' into /home/<you>/gcs_map_app/maps/maptiles.db: 1313 tiles (1313 new, 0 shared) ...  [default]
     1  POC test area                  zoom 12-18     1313 tiles      x.x MB  [default]
   ```

   Running it again just says the set is already there.

**Another place?** The robot's start point must be inside the tiles. For `make_test_mbtiles`, set **Projects > Run > Command line arguments** to `--lat 12.9716 --lon 77.5946 --radius-km 3`. Then run `import_mbtiles` with the arguments `<project>/maps/test_area.mbtiles <project>/maps/maptiles.db --default`. Use the same start point on the robot laptop.

### 5.6 Run the map app

Select **`gcs_map_app`** in the run-target selector and press **Run** (Ctrl+R). No arguments are needed: it opens `<project>/maps/maptiles.db`.

What you should see:

- **Before the robot publishes:** the map around the tile area, a "WAITING FOR DATA" badge, and "Waiting for /gcs/telemetry …" in the top bar.
- **Once `gps_sim` runs on the robot laptop** (`ros2 launch gps_sim gps_sim.launch.xml`):
  - the top bar shows **Live · 5 Hz**;
  - the map centres on the robot at the red START dot. It moves slowly north-east with a blue trail, and its arrow points the way it's going after the first couple of metres;
  - the side panel shows the latitude and longitude changing, and a wobble of about 0.5 m, like a real GPS.
- **Stop the robot's node:** within about a second the badge turns **STALE** and the arrow turns grey.

Using the map:

| Action | How |
|---|---|
| Pan | Drag (stops following the robot) |
| Zoom | Mouse wheel, double-click, or **+ / −** |
| Follow the robot again | **◎** |
| Clear the trail | **Clear trail** in the side panel |

Optional app arguments (**Projects > Run > Command line arguments**):

| Argument | Meaning |
|---|---|
| `--tiles <file>` | Another tile database or `.mbtiles` pack |
| `--zoom <z>` | Start zoom (default 17) |
| `--ros-args -p telemetry_topic:=<topic>` | Another topic |

### 5.7 If Qt Creator was started from the app menu (not a terminal)

Configuring and building still work, because `CMakeLists.txt` points at `/opt/ros/jazzy`. But `gcs_map_app` won't find ROS at run time. Give it the ROS variables:

1. In a terminal, run:

   ```bash
   source /opt/ros/jazzy/setup.bash
   env | grep -E '^(AMENT_PREFIX_PATH|LD_LIBRARY_PATH|ROS_|RMW_|PATH)='
   ```

2. In Qt Creator, with **gcs_map_app** selected: **Projects > Run > Environment**, then **Details > Batch Edit…**. Paste the lines, and add `ROS_DOMAIN_ID=21`.
3. Run again.

---

## 6. Optional: build and run from a terminal (no Qt Creator)

```bash
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=21
cmake -S ~/gcs_map_app -B ~/gcs_map_app/build -DCMAKE_BUILD_TYPE=Release
cmake --build ~/gcs_map_app/build -j
~/gcs_map_app/build/make_test_mbtiles
~/gcs_map_app/build/import_mbtiles
~/gcs_map_app/build/gcs_map_app
```

---

## 7. Troubleshooting

| Problem | Fix |
|---|---|
| CMake: `Could not find ... "ament_cmake"` / `"rclcpp"` / `"sensor_msgs"` | ROS 2 Jazzy isn't installed in `/opt/ros/jazzy` (step 1). |
| CMake: `Could not find ... "Qt6"`, or the kit shows Qt 5 | Install `qt6-base-dev qt6-declarative-dev`, and pick or add a Qt 6 kit (5.3). |
| Run: `failed to find shared library 'rmw_fastrtps_cpp'`, or `librclcpp.so: cannot open shared object file` | The run environment has no ROS. Start Qt Creator from a sourced terminal (5.1), or see 5.7. |
| `module "QtQuick.Shapes" is not installed` | `sudo apt install qml6-module-qtquick-shapes qml6-module-qtquick-window qml6-module-qtquick` |
| `QSqlDatabase: QSQLITE driver not loaded` | `sudo apt install libqt6sql6-sqlite` |
| `make_test_mbtiles`: `Could not find the Qt platform plugin "offscreen"` | `sudo apt install qt6-qpa-plugins` |
| The map says "Offline map tiles not loaded" | Run `make_test_mbtiles`, then `import_mbtiles` (5.5). |
| Grey "No map tiles here" where the robot is | The robot is outside the tile area. Make the tiles around the robot's start point (5.5). |
| Stays on "Waiting for /gcs/telemetry" | Is `gps_sim` running on the robot laptop? Run `ros2 topic hz /gcs/telemetry` in a terminal here. Check the same `ROS_DOMAIN_ID` (in Qt Creator's environment too), the firewall, and `ROS_STATIC_PEERS` (step 4). |
| Wayland warnings when the app starts | Harmless. To use X11, run `export QT_QPA_PLATFORM=xcb` before `qtcreator`. |
