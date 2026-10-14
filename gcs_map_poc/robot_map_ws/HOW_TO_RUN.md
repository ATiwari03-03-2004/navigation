# robot_map_ws: how to build and run (robot laptop)

Target: **Ubuntu 24.04 + ROS 2 Jazzy**. Everything here runs in a terminal; no Qt is needed on this laptop. The paths assume the workspace is at `~/robot_map_ws`.

---

## 1. Install (once)

ROS 2 Jazzy, if it isn't installed yet. These are the official steps from docs.ros.org:

```bash
sudo apt install software-properties-common curl
sudo add-apt-repository universe
export ROS_APT_SOURCE_VERSION=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest | grep -F "tag_name" | awk -F\" '{print $4}')
curl -L -o /tmp/ros2-apt-source.deb "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo $VERSION_CODENAME)_all.deb"
sudo dpkg -i /tmp/ros2-apt-source.deb
sudo apt update
sudo apt install ros-jazzy-ros-base ros-dev-tools
```

`ros-jazzy-ros-base` includes `rclcpp`, `sensor_msgs`, `std_msgs`, the message generators and `ros2 launch`. `ros-dev-tools` provides `colcon`.

If the commands above have changed, follow <https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html>.

---

## 2. Get the workspace

Copy the `robot_map_ws` folder to the robot laptop, for example to `~/robot_map_ws`, or clone the repository and use that folder. It must contain:

```
~/robot_map_ws/src/gps_sim/CMakeLists.txt
~/robot_map_ws/src/gps_sim/package.xml
~/robot_map_ws/src/gps_sim/src/gps_sim_node.cpp
~/robot_map_ws/src/gps_sim/config/gps_sim.yaml
~/robot_map_ws/src/gps_sim/launch/gps_sim.launch.xml
```

There is one package, and no custom message package: the node publishes the standard `sensor_msgs/msg/NavSatFix`, which comes with ROS 2.

---

## 3. The project files, explained

You don't need to edit these to run the demo. They are explained here so it's clear what each one does, and so you can recreate the workspace from scratch if needed.

### 3.1 `src/gps_sim/package.xml`: the package's identity and dependencies

`colcon` reads this file to know what the package is, how to build it, and what it needs.

```xml
<?xml version="1.0"?>
<package format="3">
  <name>gps_sim</name>
  <version>0.1.0</version>
  <description>Robot laptop: fake GPS that publishes a slowly moving position on /gcs/telemetry for the GCS map POC.</description>
  <maintainer email="you@example.com">GCS team</maintainer>
  <license>Apache-2.0</license>

  <buildtool_depend>ament_cmake</buildtool_depend>   <!-- a C++ package built with CMake + ament -->

  <depend>rclcpp</depend>                            <!-- ROS 2 C++ client library -->
  <depend>sensor_msgs</depend>                       <!-- the NavSatFix message -->

  <exec_depend>ros2launch</exec_depend>              <!-- to run the launch file -->

  <export>
    <build_type>ament_cmake</build_type>
  </export>
</package>
```

### 3.2 `src/gps_sim/CMakeLists.txt`: builds and installs the node

```cmake
cmake_minimum_required(VERSION 3.8)
project(gps_sim LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(ament_cmake REQUIRED)      # ROS 2 CMake helpers
find_package(rclcpp REQUIRED)           # ROS 2 C++
find_package(sensor_msgs REQUIRED)      # NavSatFix

add_executable(gps_sim src/gps_sim_node.cpp)
ament_target_dependencies(gps_sim rclcpp sensor_msgs)                  # include paths + libraries

install(TARGETS gps_sim RUNTIME DESTINATION lib/${PROJECT_NAME})       # for "ros2 run gps_sim gps_sim"
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})     # for "ros2 launch" and the YAML

ament_package()                                                        # registers the package with ROS 2
```

### 3.3 `src/gps_sim/config/gps_sim.yaml` and `launch/gps_sim.launch.xml`

The YAML file holds the node's parameters (section 6). The launch file starts the node with that YAML:

```xml
<launch>
  <arg name="params" default="$(find-pkg-share gps_sim)/config/gps_sim.yaml"/>
  <node pkg="gps_sim" exec="gps_sim" name="gps_sim" output="screen">
    <param from="$(var params)"/>
  </node>
</launch>
```

---

## 4. Build

```bash
cd ~/robot_map_ws
source /opt/ros/jazzy/setup.bash
colcon build --symlink-install
```

Expected output ends with:

```
Finished <<< gps_sim [..s]
Summary: 1 package finished
```

With `--symlink-install`, edits to the YAML file take effect without rebuilding.

Then, in **every new terminal**:

```bash
source /opt/ros/jazzy/setup.bash
source ~/robot_map_ws/install/setup.bash
```

---

## 5. Network setup (both laptops)

Do this on the robot laptop **and** the GCS laptop. Put the `export` lines in `~/.bashrc` so every terminal gets them.

1. Connect both laptops to the same Wi-Fi network or LAN. Note the IP address of each: `hostname -I`. Check they can reach each other: `ping <other laptop IP>`.
2. Use the same domain ID on both: `export ROS_DOMAIN_ID=21`. Any number from 0 to 101 works, as long as it's the same on both.
3. Test ROS 2 discovery between the laptops:

   ```bash
   # GCS laptop
   ros2 multicast receive
   # Robot laptop
   ros2 multicast send
   ```

   - If the GCS prints `Received from <robot IP>: 'Hello World!'`, multicast works and nothing else is needed.
   - If nothing arrives, the network blocks multicast. Point each laptop at the other with static peers instead:

     ```bash
     # robot laptop
     export ROS_STATIC_PEERS=<GCS laptop IP>
     # GCS laptop
     export ROS_STATIC_PEERS=<robot laptop IP>
     ```

4. If a firewall is on (`sudo ufw status`), allow the other laptop:

   ```bash
   sudo ufw allow from <other laptop IP>
   ```

---

## 6. Run

**With the launch file**, which uses `config/gps_sim.yaml`:

```bash
ros2 launch gps_sim gps_sim.launch.xml
```

**Or directly**, setting any parameter on the command line:

```bash
ros2 run gps_sim gps_sim
ros2 run gps_sim gps_sim --ros-args -p start_lat:=12.9716 -p start_lon:=77.5946
ros2 run gps_sim gps_sim --ros-args -p pattern:=square -p square_side_steps:=200
ros2 run gps_sim gps_sim --ros-args -p noise_m:=0.0 -p rate_hz:=10.0
```

Expected output:

```
[gps_sim]: Publishing sensor_msgs/NavSatFix on /gcs/telemetry at 5.0 Hz from 28.612900, 77.229500: pattern 'line', about 0.92 m/s, GPS noise 0.5 m
[gps_sim]: seq 25: 28.6129253, 77.2295363, heading 53.0 deg
```

| Parameter | Default | Meaning |
|---|---|---|
| `topic` | `/gcs/telemetry` | Topic to publish on |
| `frame_id` | `gps` | `header.frame_id` of every message |
| `rate_hz` | 5.0 | GPS readings per second |
| `start_lat`, `start_lon` | 28.6129, 77.2295 | Start position. **Must be inside the GCS's map tiles.** |
| `lat_step`, `lon_step` | 0.000001, 0.0000015 | Degrees added on every reading (about 0.11 m north, 0.15 m east) |
| `noise_m` | 0.5 | Random GPS error in metres (0 = exact) |
| `pattern` | `line` | `line` keeps adding the steps; `square` walks a square |
| `square_side_steps` | 300 | Readings per side of the square |
| `max_steps` | 6000 | Then go back to the start (0 = never) |
| `altitude_m` | 216.0 | |
| `drop_fix_every` | 0 | Every N readings, report "no fix" once (0 = never) |

Stop with **Ctrl+C**.

---

## 7. Check it from either laptop

```bash
ros2 topic list                                          # /gcs/telemetry
ros2 topic hz /gcs/telemetry                             # about 5 Hz
ros2 topic echo /gcs/telemetry --qos-profile sensor_data --field latitude
```

If these work **on the GCS laptop**, the network is fine, and the GCS map app will receive the positions.

---

## 8. Troubleshooting

| Problem | Fix |
|---|---|
| `colcon: command not found` | `sudo apt install ros-dev-tools` |
| `Could not find a package configuration file provided by "rclcpp"` | Run `source /opt/ros/jazzy/setup.bash` before `colcon build`. |
| `Package 'gps_sim' not found` | Run `source ~/robot_map_ws/install/setup.bash` in this terminal. |
| `ros2 launch`: file not found | Rebuild (`colcon build`): the launch and config folders are installed by the build. |
| The GCS doesn't see `/gcs/telemetry` | Use the same `ROS_DOMAIN_ID` on both, the same network, and check the firewall. Try `ROS_STATIC_PEERS` (section 5). |
| `ros2 topic echo` prints nothing | Add `--qos-profile sensor_data`: the publisher is best effort. |
| The GCS shows grey "No map tiles here" | The robot is outside the GCS's tile area. Use the same start point as the GCS's tile pack, or make a pack around your start point on the GCS. |
