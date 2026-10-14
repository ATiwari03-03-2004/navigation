// Plain C++ copy of one /gcs/telemetry message (sensor_msgs/msg/NavSatFix).
// The ROS callback fills it on the ROS thread and hands it to the
// user-interface thread, so QML never touches ROS types.
#pragma once

#include <QString>
#include <QtGlobal>

struct TelemetrySample
{
    QString frameId;           // header.frame_id set by the publisher (e.g. "gps")

    double latitude = 0.0;     // degrees, WGS 84
    double longitude = 0.0;    // degrees, WGS 84
    double altitude = 0.0;     // metres
    int fixStatus = -1;        // sensor_msgs/NavSatStatus: -1 no fix, 0 fix, 1 SBAS, 2 GBAS
    double horizontalAccuracyM = -1.0; // from the covariance; -1 if unknown

    qint64 receivedMs = 0;     // GCS clock (ms since epoch) when the message arrived
};
