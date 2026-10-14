#include "RosBridge.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QDateTime>
#include <QMetaObject>

#include "TelemetryController.h"
#include "TelemetrySample.h"

RosBridge::RosBridge(TelemetryController *target)
    : m_target(target)
{
    m_node = std::make_shared<rclcpp::Node>("gcs_map_app");
    m_topic = m_node->declare_parameter<std::string>("telemetry_topic", "/gcs/telemetry");

    // Best effort, keep last 1: only the newest position matters, and a
    // best-effort subscriber also accepts a reliable publisher (HLD 5.3.5).
    m_sub = m_node->create_subscription<sensor_msgs::msg::NavSatFix>(
        m_topic, rclcpp::SensorDataQoS(),
        [this](const sensor_msgs::msg::NavSatFix &msg) { onFix(msg); });

    m_executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    m_executor->add_node(m_node);

    RCLCPP_INFO(m_node->get_logger(), "Subscribed to %s (sensor_msgs/msg/NavSatFix)", m_topic.c_str());
}

RosBridge::~RosBridge()
{
    stop();
}

void RosBridge::start()
{
    if (m_thread.joinable())
        return;
    m_thread = std::thread([this]() {
        m_executor->spin();
        // spin() returns when stop() cancels it, or when ROS is shut down
        // (for example Ctrl+C in the terminal). In the second case, close the app too.
        if (!rclcpp::ok())
            QMetaObject::invokeMethod(qApp, []() { QCoreApplication::quit(); }, Qt::QueuedConnection);
    });
}

void RosBridge::stop()
{
    if (m_executor)
        m_executor->cancel();
    if (m_thread.joinable())
        m_thread.join();
}

// Runs on the ROS thread.
void RosBridge::onFix(const sensor_msgs::msg::NavSatFix &msg)
{
    TelemetrySample s;
    s.frameId = QString::fromStdString(msg.header.frame_id);
    s.latitude = msg.latitude;
    s.longitude = msg.longitude;
    s.altitude = msg.altitude;
    s.fixStatus = msg.status.status;
    s.receivedMs = QDateTime::currentMSecsSinceEpoch();

    // Horizontal accuracy: the larger of the east and north standard deviations.
    if (msg.position_covariance_type != sensor_msgs::msg::NavSatFix::COVARIANCE_TYPE_UNKNOWN) {
        const double v = std::max(msg.position_covariance[0], msg.position_covariance[4]);
        if (std::isfinite(v) && v >= 0.0)
            s.horizontalAccuracyM = std::sqrt(v);
    }

    // Hand the copy to the UI thread. The lambda runs there, in the target's thread.
    TelemetryController *target = m_target;
    QMetaObject::invokeMethod(
        target, [target, s]() { target->onSample(s); }, Qt::QueuedConnection);
}
