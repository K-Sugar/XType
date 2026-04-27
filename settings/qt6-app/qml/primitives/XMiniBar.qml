import QtQuick
import XType.Settings 1.0

Item {
    property real value: 0.0   // 0.0 – 1.0
    implicitHeight: 3
    implicitWidth: 100

    Rectangle {
        anchors.fill: parent
        radius: 999
        color: Theme.ink12

        Rectangle {
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            width: Math.max(0, parent.width * value)
            radius: 999
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.purpleDeep }
                GradientStop { position: 1.0; color: Theme.purple }
            }
            Behavior on width { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }
        }
    }
}
