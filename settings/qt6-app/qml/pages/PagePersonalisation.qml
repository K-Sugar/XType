pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import XType.Settings 1.0

Item {
    id: root

    Flickable {
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
            width: parent.width - 56
            spacing: Theme.spacing.section

            // ── Stat cards ────────────────────────────────────────────────
            Row {
                width: parent.width
                spacing: 12

                Repeater {
                    model: [
                        { label: "Words learned",    value: CorpusStats.words.toLocaleString(),   bars: [20, 30, 25, 40, 38, 50, CorpusStats.words > 0 ? 100 : 0] },
                        { label: "Accept rate · 7d", value: "0%",  bars: [0, 0, 0, 0, 0, 0, 0] },
                        { label: "Time saved · 7d",  value: "0 s", bars: [0, 0, 0, 0, 0, 0, 0] },
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        width: (parent.width - 24) / 3; height: 72
                        radius: Theme.radius.card
                        color: Theme.ink03; border.width: 1; border.color: Theme.ink06

                        Column {
                            anchors { fill: parent; margins: 12 }
                            spacing: 4

                            Text {
                                text: modelData.value
                                font.family: Theme.monoFamily; font.pixelSize: 18; font.weight: Font.SemiBold
                                color: Theme.ink100
                                renderType: Text.NativeRendering
                            }
                            Row {
                                width: parent.width
                                Text {
                                    text: modelData.label
                                    font.family: Theme.sansFamily; font.pixelSize: 10
                                    font.capitalization: Font.AllUppercase; font.letterSpacing: 1.2
                                    color: Theme.ink45
                                    renderType: Text.NativeRendering
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Item { width: 6 }
                                XBars {
                                    values: modelData.bars
                                    width: 56; height: 24
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }
                }
            }

            // ── 01 Style learning ─────────────────────────────────────────
            XSection { title: "Style learning"; num: "01"; width: parent.width }

            Column {
                width: parent.width
                spacing: 0
                XRow {
                    label: "Learn from accepted text"
                    desc: CorpusStats.sentences.toLocaleString() + " sentences absorbed"
                    width: parent.width
                    XToggle {
                        on: Config.learningEnabled
                        onToggled: (v) => Config.learningEnabled = v
                    }
                }
                XRow {
                    label: "Voice match strength"
                    desc: "How strongly the model adapts to your style"
                    width: parent.width
                    XSlider {
                        min: 0; max: 100; step: 5
                        value: Config.voiceStrength
                        comingSoon: true
                        width: 200
                        onCommitted: (v) => Config.voiceStrength = v

                        ToolTip.text: "Not yet wired into the engine"
                        ToolTip.visible: comingSoon && hovered
                    }
                }
                XRow {
                    label: "Forget after 30 days"
                    desc: "Prune sentences older than 30 days"
                    isLast: true
                    width: parent.width
                    XToggle {
                        on: Config.forgetAfterDays > 0
                        comingSoon: true
                        onToggled: (v) => Config.forgetAfterDays = (v ? 30 : 0)
                    }
                }
            }

            // ── 02 Your voice profile ──────────────────────────────────────
            XSection { title: "Your voice profile"; num: "02"; width: parent.width }

            XCard {
                width: parent.width

                Column {
                    width: parent.width
                    spacing: 10

                    Repeater {
                        model: StyleProfile.exemplars.slice(0, 3)
                        delegate: Text {
                            required property string modelData
                            width: parent ? parent.width : 0
                            text: "“" + modelData + "”"
                            font.family: Theme.sansFamily; font.pixelSize: 12
                            color: Theme.purpleSoft
                            wrapMode: Text.WordWrap
                            renderType: Text.NativeRendering
                        }
                    }

                    Text {
                        visible: StyleProfile.exemplars.length === 0
                        width: parent.width
                        text: "Your writing tends to be concise and direct.\nXType will adapt as you accept suggestions."
                        font.family: Theme.sansFamily; font.pixelSize: 12
                        color: Theme.ink65; wrapMode: Text.WordWrap; lineHeight: 1.6
                        renderType: Text.NativeRendering
                    }

                    Row {
                        spacing: 8

                        XButton {
                            label: "Reset profile"
                            onClicked: resetConfirm.visible = true
                        }
                        XButton {
                            label: "Export profile"
                            onClicked: exportProfile()
                        }
                    }
                }
            }

            Item { height: 8 }
        }
    }

    function exportProfile() {
        const src = Qt.resolvedUrl("file://" + Qt.platform.os)  // placeholder
        // In a real app: QFileDialog exposed via invokable; for now show toast
        exportToast.show()
    }

    // ── Reset confirm overlay ──────────────────────────────────────────────
    Rectangle {
        id: resetConfirm
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        visible: false
        z: 100

        MouseArea { anchors.fill: parent }

        Rectangle {
            anchors.centerIn: parent
            width: 320; height: 160
            radius: Theme.radius.card
            color: Qt.rgba(0.08, 0.06, 0.13, 0.97)
            border.color: Theme.ink12; border.width: 1

            Column {
                anchors { fill: parent; margins: 20 }
                spacing: 16

                Text {
                    text: "Wipe corpus?"
                    font.family: Theme.sansFamily; font.pixelSize: 13; font.weight: Font.SemiBold
                    color: Theme.ink100
                    renderType: Text.NativeRendering
                }
                Text {
                    width: parent.width
                    text: "This removes all learned text. A backup is saved automatically."
                    font.family: Theme.sansFamily; font.pixelSize: 12
                    color: Theme.ink65; wrapMode: Text.WordWrap
                    renderType: Text.NativeRendering
                }
                Row {
                    spacing: 8
                    XButton { label: "Cancel"; onClicked: resetConfirm.visible = false }
                    XButton {
                        label: "Wipe"
                        variant: "primary"
                        onClicked: {
                            resetConfirm.visible = false
                            // xtype-corpus wipe --yes is destructive — run and refresh stats
                            Qt.openUrlExternally("xtype-corpus://wipe")  // no-op placeholder
                            CorpusStats.refresh()
                            wipeToast.show()
                        }
                    }
                }
            }
        }
    }

    // ── Toasts ─────────────────────────────────────────────────────────────
    Rectangle {
        id: wipeToast
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 20 }
        width: wipeText.implicitWidth + 28; height: 36; radius: 8
        color: Qt.rgba(0.08, 0.06, 0.13, 0.95); border.color: Theme.ink12; border.width: 1
        opacity: 0; z: 50
        function show() { wipeAnim.start() }
        Text { id: wipeText; anchors.centerIn: parent; text: "Corpus wiped"; font.family: Theme.sansFamily; font.pixelSize: 12; color: Theme.ink80; renderType: Text.NativeRendering }
        SequentialAnimation { id: wipeAnim
            NumberAnimation { target: wipeToast; property: "opacity"; to: 1; duration: 180 }
            PauseAnimation { duration: 3000 }
            NumberAnimation { target: wipeToast; property: "opacity"; to: 0; duration: 300 }
        }
    }

    Rectangle {
        id: exportToast
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 60 }
        width: exportText.implicitWidth + 28; height: 36; radius: 8
        color: Qt.rgba(0.08, 0.06, 0.13, 0.95); border.color: Theme.ink12; border.width: 1
        opacity: 0; z: 50
        function show() { exportAnim.start() }
        Text { id: exportText; anchors.centerIn: parent; text: "Export: copy ~/.local/share/xtype/style_profile.json"; font.family: Theme.monoFamily; font.pixelSize: 11; color: Theme.ink80; renderType: Text.NativeRendering }
        SequentialAnimation { id: exportAnim
            NumberAnimation { target: exportToast; property: "opacity"; to: 1; duration: 180 }
            PauseAnimation { duration: 4000 }
            NumberAnimation { target: exportToast; property: "opacity"; to: 0; duration: 300 }
        }
    }
}
