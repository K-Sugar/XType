import QtQuick
import QtQuick.Window
import XType.Settings 1.0

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
        radius: Theme.radius.window
        color: Qt.rgba(0.10, 0.06, 0.22, Theme.bgAlpha)
        border.color: Theme.purpleDeep
        border.width: 1

        // Acceptance test: Theme.purple resolves
        Rectangle {
            anchors.centerIn: parent
            width: 120; height: 40
            radius: Theme.radius.btn
            color: Theme.purple

            Text {
                anchors.centerIn: parent
                text: Theme.sansFamily
                color: Theme.ink100
                font.pixelSize: 14
            }
        }

        MouseArea {
            anchors.fill: parent
            onPressed: window.startSystemMove()
        }
    }
}
