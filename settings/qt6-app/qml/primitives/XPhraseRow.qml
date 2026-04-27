import QtQuick
import XType.Settings 1.0

Item {
    id: root
    property string phrase: ""
    property bool comingSoon: false
    signal removeClicked()

    implicitHeight: 36
    implicitWidth: 300
    opacity: comingSoon ? 0.45 : 1.0
    Accessible.description: comingSoon ? "Not yet wired into the engine" : ""

    Rectangle {
        anchors.fill: parent; radius: 7
        color: rowMa.containsMouse ? Theme.ink06 : Theme.ink03
        border.width: 1; border.color: Theme.ink06
        Behavior on color { ColorAnimation { duration: 160 } }
    }

    Text {
        anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter; right: removeBtn.left; rightMargin: 8 }
        text: root.phrase
        font.family: Theme.monoFamily; font.pixelSize: 12
        color: Theme.ink80
        elide: Text.ElideRight
        renderType: Text.NativeRendering
    }

    Item {
        id: removeBtn
        anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
        width: 28; height: 28

        Text {
            anchors.centerIn: parent
            text: "×"
            font.pixelSize: 16
            color: xMa.containsMouse ? "#ff6b6b" : Theme.ink45
            renderType: Text.NativeRendering
            Behavior on color { ColorAnimation { duration: 160 } }
        }

        MouseArea {
            id: xMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.removeClicked()
        }
    }

    MouseArea {
        id: rowMa
        anchors.fill: parent
        hoverEnabled: true
    }
}
