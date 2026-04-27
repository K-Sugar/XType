import QtQuick
import QtQuick.Controls.Basic
import XType.Settings 1.0

Item {
    id: root
    property alias text: tf.text
    property alias placeholderText: tf.placeholderText
    property bool comingSoon: false
    signal editingFinished()

    implicitWidth: 240
    implicitHeight: 38
    opacity: comingSoon ? 0.45 : 1.0
    Accessible.description: comingSoon ? "Not yet wired into the engine" : ""

    Rectangle {
        anchors { fill: parent; margins: -3 }
        radius: 10
        visible: tf.activeFocus
        color: "transparent"
        border.width: 3
        border.color: Qt.rgba(0.518, 0.431, 0.918, 0.10)
    }

    Connections {
        target: tf
        function onEditingFinished() { root.editingFinished() }
    }

    TextField {
        id: tf
        anchors.fill: parent
        font.family: Theme.monoFamily; font.pixelSize: 12
        color: Theme.ink100
        placeholderTextColor: Theme.ink25
        leftPadding: 12; rightPadding: 12
        renderType: Text.NativeRendering

        background: Rectangle {
            radius: 7
            color: tf.activeFocus ? Theme.ink06 : Theme.ink03
            border.width: 1
            border.color: tf.activeFocus ? Theme.purple : Theme.ink12
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
        }
    }
}
