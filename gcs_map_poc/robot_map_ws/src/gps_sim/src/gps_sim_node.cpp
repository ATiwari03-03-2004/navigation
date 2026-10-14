// gps_sim: the robot laptop's fake GPS. Publishes sensor_msgs/msg/NavSatFix on /gcs/telemetry.
//
// NavSatFix is the standard ROS 2 GPS message (what real GPS drivers publish), so
// no custom message package is needed on either laptop.
//
// Stands in for the robot's rf_node in the map POC. Every tick it moves the
// "true" position a little (a fixed latitude/longitude step, or around a
// square) and publishes it the way a real GPS receiver would report it:
//   - a few readings per second (rate_hz, default 5 Hz, typical for GPS modules),
//   - with a small random error (noise_m, default 0.5 m), so the reported
//     position wobbles around the true path like a real fix,
//   - with fix status, accuracy (covariance) and altitude. (The GCS works out
//     the heading from the direction of travel.)
//
// Parameters (--ros-args -p name:=value, or config/gps_sim.yaml):
//   topic              /gcs/telemetry
//   frame_id           gps        header.frame_id of every message
//   rate_hz            5.0        GPS readings per second
//   start_lat          28.6129    degrees, WGS 84
//   start_lon          77.2295    degrees, WGS 84
//   lat_step           0.000001   degrees added to latitude per reading  (~0.11 m)
//   lon_step           0.0000015  degrees added to longitude per reading (~0.15 m here)
//                                 -> about 0.9 m/s north-east at 5 Hz: a walking robot
//   noise_m            0.5        standard deviation of the random GPS error, metres (0 = exact)
//   max_steps          6000       after this many readings, go back to the start (0 = never)
//   pattern            line       "line" (keep adding the steps) or "square" (walk a square)
//   square_side_steps  300        readings per side of the square
//   altitude_m         216.0
//   drop_fix_every     0          every N readings, report "no fix" once (0 = never)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <sensor_msgs/msg/nav_sat_status.hpp>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kMPerDegLat = 110574.0;    // metres per degree of latitude
constexpr double kMPerDegLonEq = 111320.0;  // metres per degree of longitude at the equator

double degToRad(double d) { return d * kPi / 180.0; }
double mPerDegLon(double lat) { return kMPerDegLonEq * std::cos(degToRad(lat)); }

// Bearing in degrees clockwise from north for a small lat/lon step.
double headingFor(double dLat, double dLon, double lat)
{
    const double northM = dLat * kMPerDegLat;
    const double eastM = dLon * mPerDegLon(lat);
    if (northM == 0.0 && eastM == 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    const double deg = std::atan2(eastM, northM) * 180.0 / kPi;
    return std::fmod(deg + 360.0, 360.0);
}
}  // namespace

class GpsSim : public rclcpp::Node
{
public:
    GpsSim() : Node("gps_sim"), rng_(std::random_device{}())
    {
        topic_ = declare_parameter<std::string>("topic", "/gcs/telemetry");
        frameId_ = declare_parameter<std::string>("frame_id", "gps");
        rateHz_ = declare_parameter<double>("rate_hz", 5.0);
        startLat_ = declare_parameter<double>("start_lat", 28.6129);
        startLon_ = declare_parameter<double>("start_lon", 77.2295);
        latStep_ = declare_parameter<double>("lat_step", 0.000001);
        lonStep_ = declare_parameter<double>("lon_step", 0.0000015);
        noiseM_ = declare_parameter<double>("noise_m", 0.5);
        maxSteps_ = declare_parameter<int>("max_steps", 6000);
        pattern_ = declare_parameter<std::string>("pattern", "line");
        side_ = std::max<int64_t>(1, declare_parameter<int>("square_side_steps", 300));
        altitude_ = declare_parameter<double>("altitude_m", 216.0);
        dropFixEvery_ = declare_parameter<int>("drop_fix_every", 0);

        if (pattern_ != "line" && pattern_ != "square") {
            RCLCPP_WARN(get_logger(), "Unknown pattern '%s', using 'line'", pattern_.c_str());
            pattern_ = "line";
        }
        if (rateHz_ <= 0.0)
            rateHz_ = 5.0;
        noiseM_ = std::max(0.0, noiseM_);

        lat_ = startLat_;
        lon_ = startLon_;
        heading_ = headingFor(latStep_, lonStep_, lat_);

        // Best effort, keep last: only the newest position matters (HLD 5.3.5).
        // The GCS subscribes with the same QoS.
        pub_ = create_publisher<sensor_msgs::msg::NavSatFix>(topic_, rclcpp::SensorDataQoS());
        timer_ = create_wall_timer(std::chrono::duration<double>(1.0 / rateHz_), [this]() { tick(); });

        const double speed = std::hypot(latStep_ * kMPerDegLat, lonStep_ * mPerDegLon(lat_)) * rateHz_;
        RCLCPP_INFO(get_logger(),
                    "Publishing sensor_msgs/NavSatFix on %s at %.1f Hz from %.6f, %.6f: pattern '%s', about %.2f m/s, GPS noise %.1f m",
                    topic_.c_str(), rateHz_, lat_, lon_, pattern_.c_str(), speed, noiseM_);
    }

private:
    void advance()
    {
        if (maxSteps_ > 0 && step_ >= maxSteps_) {
            RCLCPP_INFO(get_logger(), "max_steps reached: going back to the start position");
            lat_ = startLat_;
            lon_ = startLon_;
            step_ = 0;
        }

        double dLat = latStep_;
        double dLon = lonStep_;
        if (pattern_ == "square") {
            // North, east, south, west; each side has side_ steps of equal ground length.
            const double s = std::max(std::abs(latStep_), std::abs(lonStep_));
            const double sLon = s * kMPerDegLat / mPerDegLon(lat_);
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

        // What the GPS reports = true position + a small random error.
        double repLat = lat_, repLon = lon_;
        if (noiseM_ > 0.0) {
            std::normal_distribution<double> err(0.0, noiseM_);
            repLat += err(rng_) / kMPerDegLat;
            repLon += err(rng_) / mPerDegLon(lat_);
        }

        using sensor_msgs::msg::NavSatFix;
        using sensor_msgs::msg::NavSatStatus;
        NavSatFix msg;
        msg.header.stamp = now();
        msg.header.frame_id = frameId_;
        const bool noFix = dropFixEvery_ > 0 && (seq_ % static_cast<uint32_t>(dropFixEvery_)) == 0;
        msg.status.status = noFix ? NavSatStatus::STATUS_NO_FIX : NavSatStatus::STATUS_FIX;
        msg.status.service = NavSatStatus::SERVICE_GPS;
        msg.latitude = repLat;
        msg.longitude = repLon;
        msg.altitude = altitude_;
        // Reported accuracy: the noise level horizontally (at least 1 m, like real receivers), 2x vertically.
        const double h = std::max(1.0, noiseM_);
        msg.position_covariance = {h * h, 0.0, 0.0,
                                   0.0, h * h, 0.0,
                                   0.0, 0.0, 4.0 * h * h};
        msg.position_covariance_type = NavSatFix::COVARIANCE_TYPE_DIAGONAL_KNOWN;

        pub_->publish(msg);

        const uint32_t logEvery = static_cast<uint32_t>(std::max(1.0, rateHz_ * 5.0));  // ~every 5 s
        if (seq_ % logEvery == 0)
            RCLCPP_INFO(get_logger(), "seq %u: %.7f, %.7f, heading %.1f deg", seq_, repLat, repLon, heading_);
    }

    std::string topic_, frameId_, pattern_;
    double rateHz_, startLat_, startLon_, latStep_, lonStep_, noiseM_, altitude_;
    int64_t maxSteps_, side_, dropFixEvery_;

    double lat_ = 0.0, lon_ = 0.0, heading_ = 0.0;   // true position
    int64_t step_ = 0;
    uint32_t seq_ = 0;
    std::mt19937 rng_;

    rclcpp::Publisher<sensor_msgs::msg::NavSatFix>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<GpsSim>());
    rclcpp::shutdown();
    return 0;
}
