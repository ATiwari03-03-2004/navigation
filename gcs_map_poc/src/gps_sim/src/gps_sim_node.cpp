// Fake robot GPS: publishes gcs_interfaces/msg/Telemetry on /gcs/telemetry.
//
// Stands in for the robot's rf_node in the map POC. Every tick it moves the
// position by a fixed latitude/longitude step ("line" pattern), or walks a
// square ("square" pattern), and publishes the new position with a heading.
//
// Parameters (--ros-args -p name:=value):
//   topic              /gcs/telemetry
//   robot_id           QR-01
//   rate_hz            10.0      messages per second (the HLD telemetry rate)
//   start_lat          28.6129   degrees, WGS 84
//   start_lon          77.2295   degrees, WGS 84
//   lat_step           0.000002  degrees added to latitude per message  (line)
//   lon_step           0.000003  degrees added to longitude per message (line)
//   max_steps          3000      after this many steps, jump back to the start (0 = never)
//   pattern            line      "line" or "square"
//   square_side_steps  300       steps per side of the square
//   altitude_m         216.0
//   drop_fix_every     0         every N messages, send one with "no fix" (0 = never)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <sensor_msgs/msg/nav_sat_status.hpp>

#include <gcs_interfaces/msg/telemetry.hpp>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kMPerDegLat = 110574.0;    // metres per degree of latitude
constexpr double kMPerDegLonEq = 111320.0;  // metres per degree of longitude at the equator

double degToRad(double d) { return d * kPi / 180.0; }

// Bearing in degrees clockwise from north for a small lat/lon step.
double headingFor(double dLat, double dLon, double lat)
{
    const double northM = dLat * kMPerDegLat;
    const double eastM = dLon * kMPerDegLonEq * std::cos(degToRad(lat));
    if (northM == 0.0 && eastM == 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    const double deg = std::atan2(eastM, northM) * 180.0 / kPi;
    return std::fmod(deg + 360.0, 360.0);
}
}  // namespace

class GpsSim : public rclcpp::Node
{
public:
    GpsSim() : Node("gps_sim")
    {
        topic_ = declare_parameter<std::string>("topic", "/gcs/telemetry");
        robotId_ = declare_parameter<std::string>("robot_id", "QR-01");
        rateHz_ = declare_parameter<double>("rate_hz", 10.0);
        startLat_ = declare_parameter<double>("start_lat", 28.6129);
        startLon_ = declare_parameter<double>("start_lon", 77.2295);
        latStep_ = declare_parameter<double>("lat_step", 0.000002);
        lonStep_ = declare_parameter<double>("lon_step", 0.000003);
        maxSteps_ = declare_parameter<int>("max_steps", 3000);
        pattern_ = declare_parameter<std::string>("pattern", "line");
        side_ = std::max<int64_t>(1, declare_parameter<int>("square_side_steps", 300));
        altitude_ = declare_parameter<double>("altitude_m", 216.0);
        dropFixEvery_ = declare_parameter<int>("drop_fix_every", 0);

        if (pattern_ != "line" && pattern_ != "square") {
            RCLCPP_WARN(get_logger(), "Unknown pattern '%s', using 'line'", pattern_.c_str());
            pattern_ = "line";
        }
        if (rateHz_ <= 0.0)
            rateHz_ = 10.0;

        lat_ = startLat_;
        lon_ = startLon_;
        heading_ = headingFor(latStep_, lonStep_, lat_);

        // Best effort, keep last: only the newest position matters (HLD 5.3.5).
        // The GCS subscribes with the same QoS.
        pub_ = create_publisher<gcs_interfaces::msg::Telemetry>(topic_, rclcpp::SensorDataQoS());
        timer_ = create_wall_timer(std::chrono::duration<double>(1.0 / rateHz_), [this]() { tick(); });

        RCLCPP_INFO(get_logger(), "Publishing %s on %s at %.1f Hz, start %.6f, %.6f, pattern '%s'",
                    robotId_.c_str(), topic_.c_str(), rateHz_, lat_, lon_, pattern_.c_str());
    }

private:
    void advance()
    {
        if (maxSteps_ > 0 && step_ >= maxSteps_) {
            RCLCPP_INFO(get_logger(), "max_steps reached: jumping back to the start position");
            lat_ = startLat_;
            lon_ = startLon_;
            step_ = 0;
        }

        double dLat = latStep_;
        double dLon = lonStep_;
        if (pattern_ == "square") {
            // North, east, south, west; each side has side_ steps of equal ground length.
            const double s = std::max(std::abs(latStep_), std::abs(lonStep_));
            const double sLon = s * kMPerDegLat / (kMPerDegLonEq * std::cos(degToRad(lat_)));
            switch ((step_ / side_) % 4) {
            case 0: dLat = s;   dLon = 0.0;   break;
            case 1: dLat = 0.0; dLon = sLon;  break;
            case 2: dLat = -s;  dLon = 0.0;   break;
            default: dLat = 0.0; dLon = -sLon; break;
            }
        }

        lat_ += dLat;
        lon_ += dLon;
        ++step_;
        heading_ = headingFor(dLat, dLon, lat_);
    }

    void tick()
    {
        advance();
        ++seq_;

        gcs_interfaces::msg::Telemetry msg;
        msg.header.stamp = now();
        msg.header.frame_id = "base_link";
        msg.robot_id = robotId_;
        msg.interface_version = "1.0";
        msg.seq = seq_;

        using sensor_msgs::msg::NavSatFix;
        using sensor_msgs::msg::NavSatStatus;
        msg.gps.header.stamp = msg.header.stamp;
        msg.gps.header.frame_id = "gps";
        const bool noFix = dropFixEvery_ > 0 && (seq_ % static_cast<uint32_t>(dropFixEvery_)) == 0;
        msg.gps.status.status = noFix ? NavSatStatus::STATUS_NO_FIX : NavSatStatus::STATUS_FIX;
        msg.gps.status.service = NavSatStatus::SERVICE_GPS;
        msg.gps.latitude = lat_;
        msg.gps.longitude = lon_;
        msg.gps.altitude = altitude_;
        // 1 m horizontal, 2 m vertical standard deviation (variances on the diagonal)
        msg.gps.position_covariance = {1.0, 0.0, 0.0,
                                       0.0, 1.0, 0.0,
                                       0.0, 0.0, 4.0};
        msg.gps.position_covariance_type = NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;
        msg.heading_deg = static_cast<float>(heading_);

        pub_->publish(msg);

        const uint32_t logEvery = static_cast<uint32_t>(std::max(1.0, rateHz_ * 5.0));  // ~every 5 s
        if (seq_ % logEvery == 0)
            RCLCPP_INFO(get_logger(), "seq %u: %.6f, %.6f, heading %.1f deg", seq_, lat_, lon_, heading_);
    }

    std::string topic_, robotId_, pattern_;
    double rateHz_, startLat_, startLon_, latStep_, lonStep_, altitude_;
    int64_t maxSteps_, side_, dropFixEvery_;

    double lat_ = 0.0, lon_ = 0.0, heading_ = 0.0;
    int64_t step_ = 0;
    uint32_t seq_ = 0;

    rclcpp::Publisher<gcs_interfaces::msg::Telemetry>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<GpsSim>());
    rclcpp::shutdown();
    return 0;
}
