import QtQuick
import XType.Settings 1.0

Item {
    id: root
    property string key: ""

    implicitWidth: keyText.implicitWidth + 14
    implicitHeight: 20

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: Theme.ink06
        border.width: 1; border.color: Theme.ink12

        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1; color: Theme.ink25; radius: 4
        }
    }

    Text {
        id: keyText
        anchors.centerIn: parent
        text: root.key
        font.family: Theme.monoFamily; font.pixelSize: 10.5
        color: Theme.ink80
        renderType: Text.NativeRendering
    }
}
