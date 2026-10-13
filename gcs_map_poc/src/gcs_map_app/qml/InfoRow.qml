// One "label ........ value" line in the side panel.
import QtQuick

Item {
    id: row
    property string label
    property string value
    property color valueColor: "#e6e9ee"

    width: parent ? parent.width : 280
    height: 22

    Text {
        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
        text: row.label
        color: "#8b939e"
        font.pixelSize: 13
    }
    Text {
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        text: row.value
        color: row.valueColor
        font.pixelSize: 13
        font.family: "monospace"
        horizontalAlignment: Text.AlignRight
    }
}
