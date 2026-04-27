import QtQuick
import XType.Settings 1.0

Item {
    id: root
    property string label: ""
    property string variant: "default"   // "default" | "primary" | "ghost"
    property bool comingSoon: false
    signal clicked()

    implicitWidth: labelText.implicitWidth + 28
    implicitHeight: 32
    opacity: comingSoon ? 0.45 : 1.0
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.description: comingSoon ? "Not yet wired into the engine" : ""

    readonly property bool _hovered: btnMa.containsMouse
    readonly property bool _pressed: btnMa.pressed

    Rectangle {
        anchors.fill: parent; radius: Theme.radius.btn
        color: {
            if (root.variant === "primary")
                return root._pressed ? Qt.darker(Theme.purple, 1.1) : Theme.purple
            if (root.variant === "ghost")
                return "transparent"
            return root._pressed ? Theme.ink12 : (root._hovered ? Theme.ink06 : Theme.ink03)
        }
        border.width: root.variant === "ghost" ? 0 : 1
        border.color: root.variant === "primary" ? Theme.purple : (root._hovered ? Theme.ink25 : Theme.ink12)
        Behavior on color { ColorAnimation { duration: 160 } }
    }

    Text {
        id: labelText
        anchors.centerIn: parent
        text: root.label
        font.family: Theme.sansFamily; font.pixelSize: 13; font.weight: Font.Medium
        color: {
            if (root.variant === "primary") return "white"
            if (root.variant === "ghost") return root._hovered ? Theme.ink100 : Theme.ink45
            return root._hovered ? Theme.ink100 : Theme.ink80
        }
        renderType: Text.NativeRendering
    }

    MouseArea {
        id: btnMa
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
