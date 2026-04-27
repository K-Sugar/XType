import QtQuick
import XType.Settings 1.0

Rectangle {
    id: root
    property string cls: ""

    width: 32; height: 32; radius: 7
    border.width: 1; border.color: cls === "" ? Theme.ink12 : "transparent"

    readonly property color _g0: {
        switch (cls) {
            case "kate": return "#5d8aa8"
            case "tb":   return "#4a90d9"
            case "kons": return "#2a2a2a"
            case "fox":  return "#ff7139"
            case "disc": return "#5865f2"
            case "kde":  return Theme.purpleSoft
            default:     return Theme.ink12
        }
    }
    readonly property color _g1: {
        switch (cls) {
            case "kate": return "#3a5d77"
            case "tb":   return "#2a5a9a"
            case "kons": return "#555555"
            case "fox":  return "#c75200"
            case "disc": return "#3a4ad9"
            case "kde":  return Theme.purpleDeep
            default:     return Theme.ink06
        }
    }

    gradient: Gradient {
        orientation: Gradient.Diagonal
        GradientStop { position: 0.0; color: root._g0 }
        GradientStop { position: 1.0; color: root._g1 }
    }

    Text {
        anchors.centerIn: parent
        text: root.cls !== "" ? root.cls.slice(0, 1).toUpperCase() : "?"
        font.family: Theme.sansFamily; font.pixelSize: 14; font.weight: Font.SemiBold
        color: root.cls !== "" ? "white" : Theme.ink65
        renderType: Text.NativeRendering
    }
}
