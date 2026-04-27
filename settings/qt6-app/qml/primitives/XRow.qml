import QtQuick
import XType.Settings 1.0

Item {
    id: root
    property string label: ""
    property string desc: ""
    property bool isLast: false
    default property alias contentData: controlSlot.data

    implicitHeight: desc !== "" ? 56 : 44
    implicitWidth: 400

    Column {
        anchors { left: parent.left; verticalCenter: parent.verticalCenter; right: controlSlot.left; rightMargin: 16 }
        spacing: 3

        Text {
            text: root.label
            font.family: Theme.sansFamily; font.pixelSize: 14; font.weight: Font.Medium
            color: Theme.ink100
            renderType: Text.NativeRendering
        }

        Text {
            visible: root.desc !== ""
            text: root.desc
            font.family: Theme.sansFamily; font.pixelSize: 12
            color: Theme.ink45
            renderType: Text.NativeRendering
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }

    Item {
        id: controlSlot
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        width: childrenRect.width
        height: childrenRect.height
    }

    Rectangle {
        visible: !root.isLast
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.ink06
    }
}
