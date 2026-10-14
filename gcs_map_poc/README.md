# GCS map POC on two laptops

The robot's GPS position goes over ROS 2 to the GCS and is shown live on an offline map. Two laptops on the same network, with two separate projects:

| Laptop | Project | What runs there | Built and run with |
|---|---|---|---|
| **1. GCS laptop** | [`gcs_map_app/`](gcs_map_app/) | One Qt Creator project: the map UI (Qt 6 / QML), a ROS 2 subscriber on `/gcs/telemetry`, and the offline map tiles in SQLite (`maps/maptiles.db`), plus the two small tile tools | **Qt Creator**: open `gcs_map_app/CMakeLists.txt` |
| **2. Robot laptop** | [`robot_map_ws/`](robot_map_ws/) | One ROS 2 node, `gps_sim`, that publishes a GPS position on `/gcs/telemetry` which moves slowly, like a real GPS reading | `colcon build`, then `ros2 launch` |

```
 Robot laptop (robot_map_ws)                         GCS laptop (gcs_map_app, Qt Creator)
 ┌───────────────────────────┐   Wi-Fi / LAN         ┌───────────────────────────────────────────┐
 │ gps_sim (rclcpp)          │   ROS 2 (DDS)         │ gcs_map_app (Qt 6 + rclcpp)               │
 │  lat += lat_step          │ ──────────────────▶   │  subscriber /gcs/telemetry                │
 │  lon += lon_step          │  /gcs/telemetry       │  → map: robot arrow + trail               │
 │  + GPS noise, 5 Hz        │  sensor_msgs/msg/     │  → tiles from maps/maptiles.db (SQLite)   │
 └───────────────────────────┘  NavSatFix            └───────────────────────────────────────────┘
```

**The message is the standard `sensor_msgs/msg/NavSatFix`** (latitude, longitude, altitude, fix status, accuracy). That is what real ROS 2 GPS drivers publish, and it comes with ROS 2 itself. So neither project needs its own message package, and the GCS side is a single Qt Creator project with a single `CMakeLists.txt`.

Each project has its own `README.md` (what it does and how) and `HOW_TO_RUN.md` (every step, with the `CMakeLists.txt` and `package.xml` explained):

- GCS laptop: [gcs_map_app/README.md](gcs_map_app/README.md) and [gcs_map_app/HOW_TO_RUN.md](gcs_map_app/HOW_TO_RUN.md)
- Robot laptop: [robot_map_ws/README.md](robot_map_ws/README.md) and [robot_map_ws/HOW_TO_RUN.md](robot_map_ws/HOW_TO_RUN.md)

## Rules for the two laptops to talk

1. **Same network** and **the same `ROS_DOMAIN_ID`** on both (for example `export ROS_DOMAIN_ID=21`).
2. **ROS 2 Jazzy on both**, with the default RMW (Fast DDS).
3. **The firewall must let DDS through.** If multicast is blocked (common on office or campus Wi-Fi), set `ROS_STATIC_PEERS` to the other laptop's IP on each. The HOW_TO_RUN files explain how.
4. **The robot's start point must be inside the GCS's map tiles.** Both default to 28.6129 N, 77.2295 E.

## Quick start

```bash
# Robot laptop
cd ~/robot_map_ws && source /opt/ros/jazzy/setup.bash && colcon build && source install/setup.bash
export ROS_DOMAIN_ID=21
ros2 launch gps_sim gps_sim.launch.xml

# GCS laptop
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=21
qtcreator ~/gcs_map_app/CMakeLists.txt &
# In Qt Creator: configure with a Qt 6 kit, then run make_test_mbtiles, then import_mbtiles,
# then gcs_map_app (pick each in the run-target selector, then Ctrl+R).
```
