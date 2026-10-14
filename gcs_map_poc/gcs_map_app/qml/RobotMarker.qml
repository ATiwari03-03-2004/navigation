// The robot on the map: an arrow turned to its heading, or a dot when the
// heading is unknown. Grey when the position is stale or there is no GPS fix.
import QtQuick
import QtQuick.Shapes

Item {
    id: marker
    property real heading: 0
    property bool headingValid: true
    property bool stale: false

    width: 34
    height: 34

    readonly property color fillColor: stale ? "#8b939e" : "#1e88e5"

    // soft halo so the marker stands out on any tile
    Rectangle {
        anchors.centerIn: parent
        width: 34; height: 34; radius: 17
        color: marker.fillColor
        opacity: 0.18
    }

    // Arrow, rotated around the centre: 0 deg = north (up), clockwise.
    Shape {
        anchors.fill: parent
        visible: marker.headingValid
        rotation: marker.heading
        transformOrigin: Item.Center
        ShapePath {
            fillColor: marker.fillColor
            strokeColor: "white"
            strokeWidth: 2
            joinStyle: ShapePath.RoundJoin
            startX: 17; startY: 3
            PathLine { x: 28; y: 30 }
            PathLine { x: 17; y: 23 }
            PathLine { x: 6;  y: 30 }
            PathLine { x: 17; y: 3 }
        }
    }

    // Dot when there is no heading.
    Rectangle {
        anchors.centerIn: parent
        visible: !marker.headingValid
        width: 16; height: 16; radius: 8
        color: marker.fillColor
        border.color: "white"
        border.width: 2
    }
}
