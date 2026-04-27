import QtQuick
import XType.Settings 1.0

Item {
    id: root
    property bool accent: false
    default property alias contentData: content.data

    implicitWidth: 300
    implicitHeight: content.childrenRect.height + 28

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.card
        color: root.accent ? Qt.rgba(0.518, 0.431, 0.918, 0.10) : Theme.ink03
        border.width: 1
        border.color: root.accent ? Qt.rgba(0.518, 0.431, 0.918, 0.40) : Theme.ink06
    }

    Item {
        id: content
        anchors { fill: parent; margins: 14 }
    }
}
