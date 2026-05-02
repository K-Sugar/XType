pragma ComponentBehavior: Bound
import QtQuick
import XType.Settings 1.0

Item {
    id: root
    property bool on: false
    property bool add: false
    property string label: ""
    property bool comingSoon: false
    signal clicked()

    implicitHeight: 28
    implicitWidth: Math.max(labelText.implicitWidth + 22, 44)
    opacity: comingSoon ? 0.45 : 1.0
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.description: comingSoon ? "Not yet wired into the engine" : ""

    readonly property bool _hovered: pillMa.containsMouse

    Rectangle {
        id: bg
        anchors.fill: parent; radius: 999
        visible: !root.add
        color: root.on ? Qt.rgba(0.518, 0.431, 0.918, 0.10)
             : (root._hovered ? Theme.ink06 : Theme.ink03)
        border.width: 1
        border.color: root.on ? Qt.rgba(0.518, 0.431, 0.918, 0.5)
                   : (root._hovered ? Theme.ink25 : Theme.ink12)
        Behavior on color { ColorAnimation { duration: 160 } }
        Behavior on border.color { ColorAnimation { duration: 160 } }
    }

    Canvas {
        anchors.fill: parent
        visible: root.add
        property color strokeColor: root._hovered ? Theme.purple : Theme.ink45
        onStrokeColorChanged: requestPaint()
        onWidthChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = strokeColor.toString()
            ctx.lineWidth = 1
            ctx.setLineDash([4, 4])
            const r = height / 2
            ctx.beginPath()
            ctx.moveTo(r + 0.5, 0.5)
            ctx.lineTo(width - r - 0.5, 0.5)
            ctx.arcTo(width - 0.5, 0.5, width - 0.5, r, r)
            ctx.lineTo(width - 0.5, height - r)
            ctx.arcTo(width - 0.5, height - 0.5, width - r - 0.5, height - 0.5, r)
            ctx.lineTo(r + 0.5, height - 0.5)
            ctx.arcTo(0.5, height - 0.5, 0.5, height - r, r)
            ctx.lineTo(0.5, r)
            ctx.arcTo(0.5, 0.5, r + 0.5, 0.5, r)
            ctx.closePath()
            ctx.stroke()
        }
    }

    Text {
        id: labelText
        anchors.centerIn: parent
        text: root.label
        font.family: Theme.sansFamily; font.pixelSize: 12; font.weight: Font.Medium
        color: root.on ? Theme.purpleSoft
             : (root.add ? (root._hovered ? Theme.purpleSoft : Theme.ink45)
                         : (root._hovered ? Theme.ink100 : Theme.ink80))
        renderType: Text.NativeRendering
    }

    MouseArea {
        id: pillMa
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

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
