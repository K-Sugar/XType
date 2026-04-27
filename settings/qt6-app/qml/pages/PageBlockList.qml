import QtQuick
import QtQuick.Controls.Basic
import XType.Settings 1.0

Item {
    id: root
    anchors.fill: parent

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: col.implicitHeight + 44
        clip: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle { radius: 3; color: Theme.ink12 }
        }

        Column {
            id: col
            x: 28; y: 22
            width: flick.width - 56
            spacing: Theme.spacing.section

            // ── 01 Never suggest in… ──────────────────────────────────────
            XSection { title: "Never suggest in…"; num: "01"; width: parent.width }

            Flow {
                width: parent.width
                spacing: 6

                Repeater {
                    model: AppsKnown.curatedBlockerCandidates()
                    delegate: XPill {
                        required property var modelData
                        label: modelData.label
                        on: Config.blocklistApps.indexOf(modelData.id) >= 0
                        onClicked: {
                            let list = Config.blocklistApps.slice()
                            const idx = list.indexOf(modelData.id)
                            if (idx >= 0) list.splice(idx, 1)
                            else list.push(modelData.id)
                            Config.blocklistApps = list
                        }
                    }
                }

                XPill {
                    label: "+ add app"
                    add: true
                    onClicked: appPickerOverlay.visible = true
                }
            }

            // ── 02 Blocked phrases ────────────────────────────────────────
            XSection { title: "Blocked phrases"; num: "02"; width: parent.width }

            Row {
                width: parent.width
                spacing: 8
                XInput {
                    id: phraseInput
                    placeholderText: "Add phrase to block…"
                    width: parent.width - addBtn.width - 8
                    Keys.onReturnPressed: addPhrase()
                }
                XButton {
                    id: addBtn
                    label: "Block"
                    variant: "primary"
                    onClicked: addPhrase()
                }
            }

            Column {
                width: parent.width
                spacing: 4

                Repeater {
                    model: Config.blockedPhrases
                    delegate: XPhraseRow {
                        required property string modelData
                        required property int index
                        phrase: modelData
                        width: parent ? parent.width : 0
                        onRemoveClicked: {
                            let list = Config.blockedPhrases.slice()
                            list.splice(index, 1)
                            Config.blockedPhrases = list
                        }
                    }
                }

                Text {
                    visible: Config.blockedPhrases.length === 0
                    text: "No phrases blocked yet."
                    font.family: Theme.sansFamily; font.pixelSize: 13
                    color: Theme.ink45
                    renderType: Text.NativeRendering
                }
            }

            Item { height: 8 }
        }
    }

    function addPhrase() {
        const p = phraseInput.text.trim()
        if (p.length === 0) return
        if (Config.blockedPhrases.indexOf(p) >= 0) { phraseInput.text = ""; return }
        Config.blockedPhrases = Config.blockedPhrases.concat([p])
        phraseInput.text = ""
    }

    // ── App picker overlay ─────────────────────────────────────────────────
    Rectangle {
        id: appPickerOverlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        visible: false
        z: 100

        MouseArea { anchors.fill: parent; onClicked: appPickerOverlay.visible = false }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(400, parent.width - 48)
            height: Math.min(340, parent.height - 48)
            radius: Theme.radius.card
            color: Qt.rgba(0.08, 0.06, 0.13, 0.97)
            border.color: Theme.ink12; border.width: 1

            MouseArea { anchors.fill: parent }

            Column {
                anchors { fill: parent; margins: 16 }
                spacing: 10

                Text {
                    text: "Add to block list"
                    font.family: Theme.sansFamily; font.pixelSize: 13; font.weight: Font.SemiBold
                    color: Theme.ink100
                    renderType: Text.NativeRendering
                }

                ListView {
                    width: parent.width
                    height: parent.height - 60
                    clip: true
                    model: {
                        const all = AppsKnown.knownApps()
                        return all.filter(a => Config.blocklistApps.indexOf(a.canonical) < 0)
                    }
                    spacing: 4
                    delegate: XAppRow {
                        required property var modelData
                        appLabel: modelData.label
                        iconCls: modelData.iconCls
                        width: parent ? parent.width : 0
                        onClicked: {
                            Config.blocklistApps = Config.blocklistApps.concat([modelData.canonical])
                            appPickerOverlay.visible = false
                        }
                    }
                }

                XButton {
                    label: "Cancel"
                    width: parent.width
                    onClicked: appPickerOverlay.visible = false
                }
            }
        }
    }
}
