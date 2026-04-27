pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import XType.Settings 1.0

Item {
    id: root
    property var  options: []
    property int  selectedIndex: 0
    property bool comingSoon: false
    property var  comingSoonIndices: []    // per-option comingSoon (still fires onChange per §4 #13)
    signal selected(int index)

    implicitHeight: 30
    implicitWidth: 200
    opacity: comingSoon ? 0.45 : 1.0
    Accessible.description: comingSoon ? "Not yet wired into the engine" : ""

    Rectangle {
        anchors.fill: parent
        radius: 7
        color: Theme.ink03
        border.width: 1; border.color: Theme.ink12

        Row {
            id: segRow
            anchors { fill: parent; margins: 2 }
            spacing: 2

            Repeater {
                model: root.options
                delegate: SegItem {
                    required property string modelData
                    required property int index
                    segLabel: modelData
                    active: index === root.selectedIndex
                    optComingSoon: root.comingSoonIndices.indexOf(index) >= 0
                    height: segRow.height
                    onSegClicked: root.selected(index)
                }
            }
        }
    }

    component SegItem: Item {
        id: segRoot
        property string segLabel: ""
        property bool active: false
        property bool optComingSoon: false
        signal segClicked()

        opacity: optComingSoon ? 0.45 : 1.0
        Accessible.description: optComingSoon ? "Not yet wired into the engine" : ""

        ToolTip.text: "Not yet wired into the engine"
        ToolTip.visible: optComingSoon && segMa.containsMouse

        implicitWidth: segText.implicitWidth + 24

        Rectangle {
            anchors.fill: parent; radius: 5
            color: segRoot.active ? Qt.rgba(0.518, 0.431, 0.918, 0.10) : "transparent"
            border.width: segRoot.active ? 1 : 0
            border.color: Qt.rgba(0.518, 0.431, 0.918, 0.40)
            Behavior on color { ColorAnimation { duration: 160 } }
        }

        Text {
            id: segText
            anchors.centerIn: parent
            text: segRoot.segLabel
            font.family: Theme.sansFamily; font.pixelSize: 12; font.weight: Font.Medium
            color: segRoot.active ? Theme.purpleSoft : Theme.ink65
            renderType: Text.NativeRendering
        }

        MouseArea {
            id: segMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: segRoot.segClicked()
        }
    }
}
