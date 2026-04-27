import QtQuick
import XType.Settings 1.0

Item {
    id: root
    property string title: ""
    property string num: ""

    implicitHeight: 24
    implicitWidth: 200

    Text {
        id: numText
        visible: root.num !== ""
        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
        text: root.num
        font.family: Theme.monoFamily; font.pixelSize: 11; font.weight: Font.DemiBold
        color: Theme.purpleSoft
        renderType: Text.NativeRendering
    }

    Text {
        id: titleText
        anchors {
            left: numText.visible ? numText.right : parent.left
            leftMargin: numText.visible ? 12 : 0
            verticalCenter: parent.verticalCenter
        }
        text: root.title
        font.family: Theme.sansFamily; font.pixelSize: 11; font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1.6
        color: Theme.ink45
        renderType: Text.NativeRendering
    }

    Rectangle {
        anchors { left: titleText.right; leftMargin: 12; right: parent.right; verticalCenter: parent.verticalCenter }
        height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.ink12 }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
}
