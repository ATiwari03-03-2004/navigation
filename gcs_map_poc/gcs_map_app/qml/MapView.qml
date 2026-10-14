// The map: offline tiles, the robot's trail and the robot marker, with
// zoom, recenter, a scale bar, a freshness badge and the tile attribution.
//
// Everything that moves with the map sits in "layer". MapController gives
// tile, trail and robot positions relative to the layer's origin, and the
// layer's own position (layerX, layerY). Panning only moves the layer.
import QtQuick
import QtQuick.Shapes

Item {
    id: root
    clip: true

    function syncViewport() {
        if (width > 0 && height > 0)
            mapController.setViewportSize(width, height)
    }
    onWidthChanged: syncViewport()
    onHeightChanged: syncViewport()
    Component.onCompleted: syncViewport()

    // Background shown under tiles that are still loading.
    Rectangle {
        anchors.fill: parent
        color: "#d6d9dd"
    }

    Item {
        id: layer
        x: mapController.layerX
        y: mapController.layerY

        // Tiles: each Image asks the "maptiles" image provider (C++) for z/x/y.
        Repeater {
            model: mapController.tiles
            delegate: Image {
                x: modelData.x
                y: modelData.y
                width: 256
                height: 256
                source: modelData.url
                cache: true          // the QML pixmap cache keeps recent tiles in memory
                smooth: false
            }
        }

        // Trail since the app started (memory only).
        Shape {
            visible: mapController.trailPoints > 0
            asynchronous: true
            ShapePath {
                strokeColor: "#1565c0"
                strokeWidth: 3
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathPolyline { path: mapController.trail }
            }
        }

        RobotMarker {
            visible: mapController.hasRobot
            x: mapController.robotX - width / 2
            y: mapController.robotY - height / 2
            heading: mapController.robotHeading
            headingValid: mapController.headingValid
            stale: !mapController.robotHasFix || !telemetry.live
        }
    }

    // Drag to pan (stops following), wheel or double-click to zoom.
    MouseArea {
        anchors.fill: parent
        property real lastX: 0
        property real lastY: 0
        onPressed: (mouse) => { lastX = mouse.x; lastY = mouse.y }
        onPositionChanged: (mouse) => {
            mapController.pan(mouse.x - lastX, mouse.y - lastY)
            lastX = mouse.x
            lastY = mouse.y
        }
        onWheel: (wheel) => {
            if (wheel.angleDelta.y > 0)
                mapController.zoomAt(1, wheel.x, wheel.y)
            else if (wheel.angleDelta.y < 0)
                mapController.zoomAt(-1, wheel.x, wheel.y)
        }
        onDoubleClicked: (mouse) => mapController.zoomAt(1, mouse.x, mouse.y)
    }

    // ---------------------------------------------------------------- overlays

    // Freshness badge (top left): Live / Stale / No GPS / Waiting
    Rectangle {
        anchors { left: parent.left; top: parent.top; margins: 12 }
        width: badgeText.implicitWidth + 24
        height: 28
        radius: 14
        color: "#cc0f1216"
        Row {
            anchors.centerIn: parent
            spacing: 6
            Rectangle {
                width: 8; height: 8; radius: 4
                anchors.verticalCenter: parent.verticalCenter
                color: !telemetry.hasData ? "#8b939e"
                     : !telemetry.hasFix ? "#d29922"
                     : telemetry.live ? "#3fb950" : "#8b939e"
            }
            Text {
                id: badgeText
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                font.pixelSize: 12
                font.bold: true
                text: !telemetry.hasData ? "WAITING FOR DATA"
                    : !telemetry.hasFix ? "NO GPS FIX"
                    : telemetry.live ? "LIVE" : "STALE"
            }
        }
    }

    // Zoom and recenter (top right)
    Column {
        anchors { right: parent.right; top: parent.top; margins: 12 }
        spacing: 8
        MapButton {
            text: "◎"
            active: mapController.follow
            enabled: mapController.hasRobot
            onClicked: mapController.recenter()
        }
        MapButton {
            text: "+"
            enabled: mapController.zoom < mapController.maxZoom
            onClicked: mapController.zoomIn()
        }
        MapButton {
            text: "−"
            enabled: mapController.zoom > mapController.minZoom
            onClicked: mapController.zoomOut()
        }
    }

    // Scale bar (bottom left)
    Item {
        anchors { left: parent.left; bottom: parent.bottom; leftMargin: 14; bottomMargin: 14 }
        width: mapController.scaleBarPixels
        height: 22
        Rectangle { x: 0; y: 16; width: parent.width; height: 3; color: "#1c2128" }
        Rectangle { x: 0; y: 9; width: 2; height: 10; color: "#1c2128" }
        Rectangle { x: parent.width - 2; y: 9; width: 2; height: 10; color: "#1c2128" }
        Text {
            x: 4; y: 0
            text: mapController.scaleBarLabel + "  ·  z" + mapController.zoom
            color: "#1c2128"
            font.pixelSize: 11
            font.bold: true
        }
    }

    // Attribution (bottom right): the tile pack's source and licence.
    Rectangle {
        anchors { right: parent.right; bottom: parent.bottom }
        visible: tileStore.ready && tileStore.attribution.length > 0
        width: attrText.implicitWidth + 12
        height: attrText.implicitHeight + 6
        color: "#bfffffff"
        Text {
            id: attrText
            anchors.centerIn: parent
            text: tileStore.attribution
            textFormat: Text.PlainText
            color: "#333"
            font.pixelSize: 10
        }
    }

    // No tile database: say why, and how to fix it.
    Rectangle {
        anchors.centerIn: parent
        visible: !tileStore.ready
        width: Math.min(parent.width - 40, 560)
        height: errText.implicitHeight + 32
        radius: 8
        color: "#e6171b21"
        Text {
            id: errText
            anchors { fill: parent; margins: 16 }
            wrapMode: Text.WordWrap
            color: "#e6e9ee"
            font.pixelSize: 13
            text: "Offline map tiles not loaded.\n" + tileStore.error +
                  "\n\nIn Qt Creator, run make_test_mbtiles, then import_mbtiles, " +
                  "then start the app with --tiles <path to maptiles.db> (see HOW_TO_RUN.md). " +
                  "The robot position is still tracked on a grey grid."
        }
    }
}
