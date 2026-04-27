pragma ComponentBehavior: Bound
import QtQuick
import XType.Settings 1.0

Item {
    id: root
    implicitHeight: 220
    implicitWidth: 400

    function appendLine(text, kind) {
        if (logModel.count >= 200) logModel.remove(0)
        logModel.append({ "line": text, "kind": kind || "normal" })
    }

    function clearLines() { logModel.clear() }

    ListModel { id: logModel }

    Rectangle {
        anchors.fill: parent; radius: 8
        color: Qt.rgba(0, 0, 0, 0.35)
        border.width: 1; border.color: Theme.ink06

        ListView {
            id: lv
            anchors { fill: parent; margins: 12 }
            clip: true
            model: logModel
            spacing: 0

            onCountChanged: Qt.callLater(positionViewAtEnd)

            delegate: Text {
                required property string line
                required property string kind
                width: lv.width
                text: line
                font.family: Theme.monoFamily; font.pixelSize: 11
                lineHeight: 1.75
                wrapMode: Text.WrapAnywhere
                color: kind === "accent" ? Theme.purpleSoft
                     : kind === "err"    ? "#ff8a8a"
                     : Theme.ink65
                renderType: Text.NativeRendering
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {
                    radius: 3
                    color: Theme.ink12
                }
            }
        }
    }
}
