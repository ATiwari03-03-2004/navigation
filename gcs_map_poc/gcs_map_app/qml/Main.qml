// Main window: top bar, live map, and a telemetry side panel.
// Views only show what the controllers expose and send user actions to them
// (HLD rule R1): no ROS, SQL or projection code lives in QML.
import QtQuick
import QtQuick.Window

Window {
    id: win
    width: 1280
    height: 800
    visible: true
    title: "RoboGCS – Map POC (offline tiles)"
    color: "#0f1216"

    readonly property color panelColor: "#171b21"
    readonly property color lineColor: "#262c35"
    readonly property color textColor: "#e6e9ee"
    readonly property color dimColor: "#8b939e"
    readonly property color liveColor: "#3fb950"
    readonly property color warnColor: "#d29922"

    // ---------------------------------------------------------------- top bar
    Rectangle {
        id: topBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 56
        color: win.panelColor

        Column {
            anchors { left: parent.left; leftMargin: 18; verticalCenter: parent.verticalCenter }
            Text { text: "RoboGCS"; color: win.textColor; font.pixelSize: 18; font.bold: true }
            Text { text: "Map POC · offline tiles · ROS 2 " + telemetryTopic; color: win.dimColor; font.pixelSize: 12 }
        }

        // Link status: green Live / amber Stale / grey Waiting
        Row {
            anchors { right: parent.right; rightMargin: 18; verticalCenter: parent.verticalCenter }
            spacing: 8
            Rectangle {
                width: 10; height: 10; radius: 5
                anchors.verticalCenter: parent.verticalCenter
                color: telemetry.live ? win.liveColor : (telemetry.hasData ? win.warnColor : win.dimColor)
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: win.textColor
                font.pixelSize: 14
                text: telemetry.live ? "Live · " + telemetry.rateHz.toFixed(0) + " Hz"
                      : telemetry.hasData ? "Stale · last message " + telemetry.ageSeconds.toFixed(1) + " s ago"
                      : "Waiting for " + telemetryTopic + " …"
            }
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: win.lineColor
        }
    }

    // ---------------------------------------------------------------- map
    MapView {
        id: mapView
        anchors { left: parent.left; top: topBar.bottom; bottom: parent.bottom; right: sidePanel.left }
    }

    // ---------------------------------------------------------------- side panel
    Rectangle {
        id: sidePanel
        width: 320
        anchors { right: parent.right; top: topBar.bottom; bottom: parent.bottom }
        color: win.panelColor
        Rectangle {
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            width: 1
            color: win.lineColor
        }

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: 16
            contentHeight: col.height
            clip: true

            Column {
                id: col
                width: flick.width
                spacing: 6

                Text { text: "Robot"; color: win.textColor; font.pixelSize: 16; font.bold: true }
                InfoRow { label: "Message"; value: "sensor_msgs/NavSatFix" }
                InfoRow { label: "Frame ID"; value: telemetry.hasData ? telemetry.frameId : "—" }
                InfoRow { label: "Last update"; value: telemetry.lastUpdateText }
                InfoRow { label: "Messages"; value: telemetry.messageCount.toFixed(0) }

                Item { width: 1; height: 10 }
                Text { text: "GPS"; color: win.textColor; font.pixelSize: 16; font.bold: true }
                InfoRow {
                    label: "Fix"
                    value: !telemetry.hasData ? "—" : (telemetry.hasFix ? "Fix" : "No fix")
                    valueColor: telemetry.hasData && !telemetry.hasFix ? win.warnColor : win.textColor
                }
                InfoRow { label: "Latitude"; value: telemetry.hasData ? telemetry.latitude.toFixed(7) + "°" : "—" }
                InfoRow { label: "Longitude"; value: telemetry.hasData ? telemetry.longitude.toFixed(7) + "°" : "—" }
                InfoRow { label: "Altitude"; value: telemetry.hasData ? telemetry.altitude.toFixed(1) + " m" : "—" }
                InfoRow { label: "Accuracy"; value: telemetry.hasData && telemetry.accuracyM >= 0 ? "± " + telemetry.accuracyM.toFixed(1) + " m" : "—" }
                InfoRow { label: "Heading (travel)"; value: telemetry.headingValid ? telemetry.heading.toFixed(1) + "°" : "—" }

                Item { width: 1; height: 10 }
                Text { text: "Map"; color: win.textColor; font.pixelSize: 16; font.bold: true }
                InfoRow { label: "Zoom"; value: mapController.zoom + "  (" + mapController.minZoom + "–" + mapController.maxZoom + ")" }
                InfoRow { label: "Centre"; value: mapController.centerLat.toFixed(5) + ", " + mapController.centerLon.toFixed(5) }
                InfoRow { label: "Follow robot"; value: mapController.follow ? "On" : "Off (tap ◎)" }
                InfoRow { label: "Trail"; value: mapController.trailPoints + " points, " + mapController.trailLengthM.toFixed(0) + " m" }

                Item { width: 1; height: 10 }
                Text { text: "Tiles (SQLite)"; color: win.textColor; font.pixelSize: 16; font.bold: true }
                InfoRow { label: "Tile set"; value: tileStore.ready ? tileStore.name : "—" }
                InfoRow { label: "Layout"; value: tileStore.layout }
                InfoRow { label: "Zoom levels"; value: tileStore.ready ? tileStore.minZoom + "–" + tileStore.maxZoom : "—" }
                InfoRow { label: "Tiles"; value: tileStore.ready ? tileStore.tileCount.toFixed(0) : "—" }
                Text {
                    width: parent.width
                    text: tileStore.path
                    color: win.dimColor
                    font.pixelSize: 11
                    wrapMode: Text.WrapAnywhere
                }

                Item { width: 1; height: 10 }
                Rectangle {
                    width: parent.width; height: 34; radius: 6
                    color: clearArea.pressed ? "#2b323c" : "#222831"
                    border.color: win.lineColor
                    Text { anchors.centerIn: parent; text: "Clear trail"; color: win.textColor; font.pixelSize: 13 }
                    MouseArea { id: clearArea; anchors.fill: parent; onClicked: mapController.clearTrail() }
                }
            }
        }
    }
}
