# Telemetry published by the robot's rf_node on /gcs/telemetry.
# Map POC subset of the HLD message (Section 5.3.5, Table 5.3-5):
# only the fields the map needs, plus identity and sequence.

std_msgs/Header header         # robot time of the sample

string robot_id                # e.g. "QR-01"; the GCS checks it on every message
string interface_version       # "MAJOR.MINOR", e.g. "1.0"
uint32 seq                     # increments by 1 per message; gaps = lost messages

sensor_msgs/NavSatFix gps      # WGS 84 latitude/longitude/altitude, fix status, covariance

float32 heading_deg            # degrees clockwise from true north, NaN if unknown
