// RosBridge: the GCS's ROS 2 listener.
//
// Owns one rclcpp node ("gcs_map_app") that subscribes to /gcs/telemetry
// (sensor_msgs/msg/NavSatFix: the robot's GPS fix), and a single-threaded
// executor spinning on its own std::thread, so ROS never blocks the Qt
// user-interface thread. Every message is copied into a TelemetrySample and
// posted to the TelemetryController on the UI thread with a queued call.
#pragma once

#include <memory>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>

class TelemetryController;

class RosBridge
{
public:
    explicit RosBridge(TelemetryController *target);
    ~RosBridge();

    RosBridge(const RosBridge &) = delete;
    RosBridge &operator=(const RosBridge &) = delete;

    void start();   // start spinning on the ROS thread
    void stop();    // stop spinning and join the ROS thread

    std::string topic() const { return m_topic; }

private:
    void onFix(const sensor_msgs::msg::NavSatFix &msg);

    TelemetryController *m_target;  // lives on the UI thread, outlives the bridge
    std::shared_ptr<rclcpp::Node> m_node;
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr m_sub;
    std::shared_ptr<rclcpp::executors::SingleThreadedExecutor> m_executor;
    std::thread m_thread;
    std::string m_topic;
};
