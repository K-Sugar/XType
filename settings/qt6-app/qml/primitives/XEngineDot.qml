import QtQuick
import XType.Settings 1.0

Item {
    id: root
    width: 15; height: 15
    property bool active: true

    // Glow halo
    Rectangle {
        anchors.centerIn: parent
        width: 14; height: 14; radius: 7
        color: Qt.rgba(0.706, 0.671, 1.0, 0.25)
    }

    Rectangle {
        anchors.centerIn: parent
        width: 7; height: 7; radius: 3.5
        color: Theme.purpleSoft
    }

    SequentialAnimation on opacity {
        running: root.active
        loops: Animation.Infinite
        NumberAnimation { to: 0.6; duration: 1200; easing.type: Easing.InOutSine }
        NumberAnimation { to: 1.0; duration: 1200; easing.type: Easing.InOutSine }
    }
}
