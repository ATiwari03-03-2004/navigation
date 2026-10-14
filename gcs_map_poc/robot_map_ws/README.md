# robot_map_ws: the robot laptop's fake GPS

This workspace runs on the **robot laptop**. It has one ROS 2 node, `gps_sim`, that pretends to be the robot's GPS. A few times a second, it publishes the robot's position on **`/gcs/telemetry`** as a standard **`sensor_msgs/msg/NavSatFix`**. The position moves slowly, like a walking robot, and wobbles a little, like a real GPS reading. The GCS laptop (`gcs_map_app`) subscribes to the topic and draws the position on its map.

Steps to build and run: [HOW_TO_RUN.md](HOW_TO_RUN.md).

## Contents

```
robot_map_ws/
├── src/
│   └── gps_sim/                     the fake GPS node (C++, rclcpp)
│       ├── src/gps_sim_node.cpp
│       ├── config/gps_sim.yaml      its parameters
│       ├── launch/gps_sim.launch.xml
│       ├── CMakeLists.txt
│       └── package.xml
├── README.md
└── HOW_TO_RUN.md
```

## The message: `sensor_msgs/msg/NavSatFix`

This is the standard ROS 2 GPS message, the one real GPS drivers publish. It comes with ROS 2, so no message package has to be built on either laptop.

```
std_msgs/Header header            # stamp, frame_id ("gps")
NavSatStatus status               # status: -1 no fix, 0 fix (+ SBAS/GBAS); service: GPS
float64 latitude                  # degrees, WGS 84
float64 longitude                 # degrees, WGS 84
float64 altitude                  # metres
float64[9] position_covariance    # accuracy: variances in m² on the diagonal
uint8 position_covariance_type    # DIAGONAL_KNOWN here
```

NavSatFix carries no heading. The GCS works the heading out from the direction of travel.

## How `gps_sim` behaves

At every tick (5 per second by default, which is typical for GPS modules):

1. **Move the true position by a small fixed step:** `lat += lat_step`, `lon += lon_step`.
   - The defaults are 0.000001° and 0.0000015°, about 0.11 m north and 0.15 m east per reading.
   - At 5 Hz that is about 0.9 m/s towards the north-east, roughly a walking robot.
   - With `pattern:=square`, it walks a square instead and keeps coming back.
2. **Add GPS noise:** a random error with a standard deviation of `noise_m` (0.5 m by default) is added to the reported position, so it wobbles around the true path like a real fix. With `noise_m:=0.0` the positions are exact.
3. **Fill in the rest of the NavSatFix:**
   - the time stamp and `frame_id`;
   - fix status `STATUS_FIX` (or "no fix" every N readings with `drop_fix_every`, to test the GCS);
   - the altitude;
   - the accuracy, as the covariance.
4. **Publish** on `/gcs/telemetry` with best-effort QoS, keep last (`SensorDataQoS`).
   - Only the newest position matters, and this is the QoS the GCS subscribes with.
   - Telemetry over a radio should drop old samples, not resend them (HLD 5.3.5).

After `max_steps` readings (6,000, about 20 minutes) it goes back to the start, so a long demo stays on the GCS's map tiles. It logs its position every 5 s.

All of this is set in `config/gps_sim.yaml`, or on the command line with `--ros-args -p name:=value`. HOW_TO_RUN.md, step 6, lists every parameter.

## Note for later

The HLD's full telemetry message (Section 5.3.5) carries more than GPS: robot ID, sequence number, heading, battery, joints and so on. That needs a custom message package (`gcs_interfaces`), built on both laptops. For this map POC, NavSatFix carries everything the map needs, which keeps both projects to a single package each.
