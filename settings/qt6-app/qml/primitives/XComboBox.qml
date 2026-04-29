import QtQuick
import QtQuick.Controls.Basic
import XType.Settings 1.0

Item {
    id: root

    property alias model:        cb.model
    property alias currentIndex: cb.currentIndex
    property alias currentText:  cb.currentText
    property alias displayText:  cb.displayText

    signal activated(int index)

    implicitWidth:  200
    implicitHeight: 32

    // Focus ring — drawn outside the button bounds, same style as XInput
    Rectangle {
        anchors { fill: cb; margins: -3 }
        radius: 10
        visible: cb.activeFocus
        color: "transparent"
        border.width: 3
        border.color: Qt.rgba(0.518, 0.431, 0.918, 0.10)
    }

    ComboBox {
        id: cb
        anchors.fill: parent
        onActivated: (i) => root.activated(i)

        // ── Closed-state button ──────────────────────────────────────────

        background: Rectangle {
            radius: 7
            color: cb.pressed ? Theme.ink06 : Theme.ink03
            border.width: 1
            border.color: (cb.hovered || cb.activeFocus) ? Theme.ink25 : Theme.ink12
            Behavior on color       { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
        }

        contentItem: Text {
            leftPadding:  12
            rightPadding: cb.indicator.width + 8
            text: cb.displayText
            font.family:  Theme.sansFamily
            font.pixelSize: 12
            color: Theme.ink100
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            renderType: Text.NativeRendering
        }

        indicator: Text {
            x: cb.width - width - 10
            y: (cb.height - height) / 2 + 1
            text: "▾"
            font.pixelSize: 13
            color: Theme.ink45
            renderType: Text.NativeRendering
        }

        // ── Popup ────────────────────────────────────────────────────────

        popup: Popup {
            y: cb.height + 4
            width: cb.width
            implicitHeight: Math.min(listView.contentHeight + 2, 224)
            padding: 1

            background: Rectangle {
                radius: 8
                color: Qt.rgba(0.10, 0.08, 0.18, 1.0)
                border.width: 1
                border.color: Theme.ink25
            }

            contentItem: ListView {
                id: listView
                clip: true
                model: cb.delegateModel
                currentIndex: cb.highlightedIndex

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle { radius: 3; color: Theme.ink25 }
                }
            }
        }

        // ── Delegate ─────────────────────────────────────────────────────

        delegate: ItemDelegate {
            id: del
            width: ListView.view ? ListView.view.width : 0
            implicitHeight: 34
            highlighted: cb.highlightedIndex === index

            background: Rectangle {
                color: del.highlighted ? Theme.purpleTint
                     : del.hovered     ? Theme.ink06
                     :                   "transparent"
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            contentItem: Text {
                leftPadding: 12
                text: modelData
                font.family:  Theme.sansFamily
                font.pixelSize: 12
                color: del.highlighted ? Theme.purpleSoft : Theme.ink80
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                renderType: Text.NativeRendering
                Behavior on color { ColorAnimation { duration: 120 } }
            }
        }
    }
}
