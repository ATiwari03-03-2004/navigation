# GCS map POC: ONE Qt Creator project (File > Open File or Project > this CMakeLists.txt).
#
# It builds three programs:
#   gcs_map_app         the GCS map UI + ROS 2 subscriber on /gcs/telemetry
#   make_test_mbtiles   draws a test offline tile pack (maps/test_area.mbtiles)
#   import_mbtiles      imports a tile pack into the SQLite tile database (maps/maptiles.db)
#
# The robot publishes sensor_msgs/msg/NavSatFix (a standard ROS 2 message), so
# no custom message package is needed: everything is in this one folder.

cmake_minimum_required(VERSION 3.16)
project(gcs_map_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

# ---------------------------------------------------------------- dependencies
# ROS 2 Jazzy, from /opt/ros/jazzy. The hint lets Qt Creator configure the
# project even if it was not started from a terminal where ROS is sourced.
list(APPEND CMAKE_PREFIX_PATH "/opt/ros/jazzy")
find_package(ament_cmake REQUIRED)    # ROS 2 CMake helpers (needed by the ROS packages below)
find_package(rclcpp REQUIRED)          # ROS 2 C++ client library (the subscriber)
find_package(sensor_msgs REQUIRED)     # sensor_msgs/msg/NavSatFix (latitude, longitude)

# Qt 6.4+ (Ubuntu 24.04 ships 6.4.2). Sql = SQLite tile database, Gui = drawing test tiles.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Sql)
qt_standard_project_setup()            # turns on AUTOMOC for Q_OBJECT classes

# Where the tile files live: <this folder>/maps. Compiled into the programs as
# their default, so nothing has to be typed in Qt Creator's run settings.
set(GCS_MAPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/maps")

# ---------------------------------------------------------------- the GCS map app
qt_add_executable(gcs_map_app
  src/main.cpp
  src/GeoMath.h
  src/TelemetrySample.h
  src/RosBridge.h            src/RosBridge.cpp
  src/TelemetryController.h  src/TelemetryController.cpp
  src/MapController.h        src/MapController.cpp
  src/TileStore.h            src/TileStore.cpp
  src/TileImageProvider.h    src/TileImageProvider.cpp
)

# QML files are compiled into the binary as Qt resources (qrc:/qml/...): rebuild after editing them.
qt_add_resources(gcs_map_app "gcs_map_qml"
  PREFIX "/"
  FILES
    qml/Main.qml
    qml/MapView.qml
    qml/RobotMarker.qml
    qml/MapButton.qml
    qml/InfoRow.qml
)

target_compile_definitions(gcs_map_app PRIVATE
  QT_NO_KEYWORDS                                     # use Q_SIGNALS/Q_EMIT; avoids clashes with other headers
  GCS_MAPS_DIR="${GCS_MAPS_DIR}")
target_link_libraries(gcs_map_app PRIVATE
  Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Sql
  rclcpp::rclcpp)
if(DEFINED sensor_msgs_TARGETS)
  target_link_libraries(gcs_map_app PRIVATE ${sensor_msgs_TARGETS})
else()
  ament_target_dependencies(gcs_map_app PUBLIC sensor_msgs)
endif()

# ---------------------------------------------------------------- tile tools (Qt only, no ROS)
qt_add_executable(make_test_mbtiles tools/make_test_mbtiles.cpp tools/TileMath.h)
target_compile_definitions(make_test_mbtiles PRIVATE GCS_MAPS_DIR="${GCS_MAPS_DIR}")
target_link_libraries(make_test_mbtiles PRIVATE Qt6::Core Qt6::Gui Qt6::Sql)

qt_add_executable(import_mbtiles tools/import_mbtiles.cpp tools/TileMath.h)
target_compile_definitions(import_mbtiles PRIVATE GCS_MAPS_DIR="${GCS_MAPS_DIR}")
target_link_libraries(import_mbtiles PRIVATE Qt6::Core Qt6::Sql)
