// Square button used on the map (zoom in, zoom out, recenter).
import QtQuick

Rectangle {
    id: btn
    property string text
    property bool active: false
    signal clicked()

    width: 40
    height: 40
    radius: 6
    color: area.pressed ? "#2f6fd6" : (active ? "#1f5bbf" : "#1c2128")
    border.color: active ? "#4c8df6" : "#30363d"
    opacity: enabled ? 0.95 : 0.4

    Text {
        anchors.centerIn: parent
        text: btn.text
        color: "#e6e9ee"
        font.pixelSize: 20
        font.bold: true
    }
    MouseArea {
        id: area
        anchors.fill: parent
        onClicked: btn.clicked()
    }
}
