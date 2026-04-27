import QtQuick
import XType.Settings 1.0

Item {
    id: root
    property string appLabel: ""
    property string appSub: ""
    property string iconCls: ""
    property bool selected: false
    property bool comingSoon: false
    default property alias contentData: controlSlot.data
    signal clicked()

    implicitHeight: 56
    implicitWidth: 300
    opacity: comingSoon ? 0.45 : 1.0
    Accessible.description: comingSoon ? "Not yet wired into the engine" : ""

    Rectangle {
        anchors.fill: parent; radius: 8
        color: root.selected ? Qt.rgba(0.518, 0.431, 0.918, 0.10)
             : (rowMa.containsMouse ? Theme.ink06 : Theme.ink03)
        border.width: 1
        border.color: root.selected ? Theme.purple
                   : (rowMa.containsMouse ? Theme.ink12 : Theme.ink06)
        Behavior on color { ColorAnimation { duration: 160 } }
        Behavior on border.color { ColorAnimation { duration: 160 } }
    }

    Row {
        anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
        spacing: 12

        XAppIcon { cls: root.iconCls; anchors.verticalCenter: parent.verticalCenter }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: root.appLabel
                font.family: Theme.sansFamily; font.pixelSize: 13; font.weight: Font.Medium
                color: Theme.ink100
                renderType: Text.NativeRendering
            }
            Text {
                visible: root.appSub !== ""
                text: root.appSub
                font.family: Theme.sansFamily; font.pixelSize: 11
                color: Theme.ink45
                renderType: Text.NativeRendering
            }
        }
    }

    Item {
        id: controlSlot
        anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
        width: childrenRect.width; height: childrenRect.height
    }

    MouseArea {
        id: rowMa
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
