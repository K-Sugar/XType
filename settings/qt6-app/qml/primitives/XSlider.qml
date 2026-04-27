import QtQuick
import QtQuick.Controls.Basic
import XType.Settings 1.0

Item {
    id: root
    property real value: 50
    property real min: 0
    property real max: 100
    property real step: 1
    property bool comingSoon: false
    signal committed(real v)

    implicitWidth: 220
    implicitHeight: 22
    opacity: comingSoon ? 0.45 : 1.0
    Accessible.description: comingSoon ? "Not yet wired into the engine" : ""

    readonly property real _pct: max > min ? (value - min) / (max - min) : 0
    readonly property alias hovered: sliderMa.containsMouse

    Text {
        id: valueLabel
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        width: 56
        text: Math.round(root.value)
        font.family: Theme.monoFamily; font.pixelSize: 11
        color: Theme.ink65
        horizontalAlignment: Text.AlignRight
        renderType: Text.NativeRendering
    }

    Item {
        id: trackArea
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; right: valueLabel.left; rightMargin: 10 }

        Rectangle {
            id: track
            height: 4; radius: 999; width: parent.width
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.ink12

            Rectangle {
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                width: Math.max(0, root._pct * parent.width)
                radius: 999
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.purpleDeep }
                    GradientStop { position: 1.0; color: Theme.purple }
                }
            }
        }

        Rectangle {
            width: 14; height: 14; radius: 7
            anchors.verticalCenter: track.verticalCenter
            x: Math.max(-7, Math.min(trackArea.width - 7, root._pct * trackArea.width - 7))
            color: Theme.ink100
            border.width: 2; border.color: Theme.purple
        }

        MouseArea {
            id: sliderMa
            anchors { fill: parent; topMargin: -6; bottomMargin: -6 }
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onPressed: (mouse) => { root._update(mouse.x); debounce.restart() }
            onPositionChanged: (mouse) => { if (pressed) { root._update(mouse.x); debounce.restart() } }
            onReleased: { debounce.stop(); root.committed(root.value) }
        }
    }

    Timer {
        id: debounce
        interval: 500
        onTriggered: root.committed(root.value)
    }

    function _update(mx) {
        const p = Math.max(0, Math.min(1, mx / trackArea.width))
        const raw = root.min + p * (root.max - root.min)
        root.value = Math.max(root.min, Math.min(root.max, Math.round(raw / root.step) * root.step))
    }
}
