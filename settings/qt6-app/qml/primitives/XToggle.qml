import QtQuick
import XType.Settings 1.0

Item {
    id: root
    property bool on: false
    property bool comingSoon: false
    signal toggled(bool checked)

    implicitWidth: 36
    implicitHeight: 20
    opacity: comingSoon ? 0.45 : 1.0
    activeFocusOnTab: true
    Accessible.role: Accessible.CheckBox
    Accessible.checked: on
    Accessible.description: comingSoon ? "Not yet wired into the engine" : ""

    Rectangle {
        anchors.fill: parent
        radius: 999
        color: root.on ? Qt.rgba(0.518, 0.431, 0.918, 0.20) : Theme.ink12
        border.width: 1
        border.color: root.on ? Theme.purple : Theme.ink12
        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }

        Rectangle {
            width: 14; height: 14; radius: 7
            y: 2
            x: root.on ? 20 : 2
            color: root.on ? Theme.purple : Theme.ink65
            Behavior on x {
                NumberAnimation {
                    duration: 220
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.34, 1.4, 0.6, 1.0, 1.0, 1.0]
                }
            }
            Behavior on color { ColorAnimation { duration: 200 } }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.on)
    }

    Keys.onSpacePressed: root.toggled(!root.on)

    Rectangle {
        anchors.fill: parent
        anchors.margins: -2
        radius: 999
        color: "transparent"
        border.color: Theme.purple
        border.width: 2
        visible: root.activeFocus
        z: 10
    }
}
