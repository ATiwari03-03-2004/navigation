cmake_minimum_required(VERSION 3.16)
project(gcs_map_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

# ROS 2 (Jazzy): found through the sourced environment (source /opt/ros/jazzy/setup.bash
# and the workspace's install/setup.bash, so gcs_interfaces is found too).
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(gcs_interfaces REQUIRED)

# Qt 6.4+ (Ubuntu 24.04 ships 6.4.2). Sql is for the SQLite tile database.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Sql)
qt_standard_project_setup()   # turns on AUTOMOC

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

# QML files are compiled into the binary as Qt resources (qrc:/qml/...).
qt_add_resources(gcs_map_app "gcs_map_qml"
  PREFIX "/"
  FILES
    qml/Main.qml
    qml/MapView.qml
    qml/RobotMarker.qml
    qml/MapButton.qml
    qml/InfoRow.qml
)

# Qt's "signals/slots/emit" keywords can clash with other headers; use
# Q_SIGNALS / Q_SLOTS / Q_EMIT instead.
target_compile_definitions(gcs_map_app PRIVATE QT_NO_KEYWORDS)

target_link_libraries(gcs_map_app PRIVATE
  Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Sql
  rclcpp::rclcpp
)

# Generated message type support for gcs_interfaces.
if(DEFINED gcs_interfaces_TARGETS)
  target_link_libraries(gcs_map_app PRIVATE ${gcs_interfaces_TARGETS})
else()
  ament_target_dependencies(gcs_map_app PUBLIC gcs_interfaces)
endif()

# Installed where "ros2 run gcs_map_app gcs_map_app" looks for it.
install(TARGETS gcs_map_app RUNTIME DESTINATION lib/${PROJECT_NAME})

ament_package()
