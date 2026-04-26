import QtQuick
import QtQuick.Window

Window {
    id: window
    width: 400
    height: 300
    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "XType Settings"

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: "#1a1025"
        border.color: "#3d2b6e"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "XType Settings"
            color: "#e0d8f8"
            font.pixelSize: 18
        }

        MouseArea {
            id: dragArea
            anchors.fill: parent
            onPressed: window.startSystemMove()
        }
    }
}
