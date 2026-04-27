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

            // ── Brand card ─────────────────────────────────────────────────
            XCard {
                width: parent.width

                Row {
                    spacing: 16
                    width: parent.width

                    Rectangle {
                        width: 64; height: 64; radius: 14
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#9b88ff" }
                            GradientStop { position: 1.0; color: "#35409d" }
                        }
                        Text {
                            anchors.centerIn: parent; text: "X"; color: "white"
                            font.family: Theme.sansFamily; font.pixelSize: 28; font.weight: Font.Bold
                            renderType: Text.NativeRendering
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4
                        Text {
                            text: "XType"
                            font.family: Theme.sansFamily; font.pixelSize: 20; font.weight: Font.DemiBold
                            color: Theme.ink100
                            renderType: Text.NativeRendering
                        }
                        Text {
                            text: "v0.1.0  ·  Fcitx5 engine  ·  MIT License"
                            font.family: Theme.monoFamily; font.pixelSize: 11
                            color: Theme.ink45
                            renderType: Text.NativeRendering
                        }
                        Row {
                            spacing: 8
                            XButton {
                                label: "GitHub"
                                onClicked: Qt.openUrlExternally("https://github.com/K-Sugar/XType")
                            }
                            XButton {
                                label: "Release notes"
                                onClicked: Qt.openUrlExternally("https://github.com/K-Sugar/XType/releases")
                            }
                        }
                    }
                }
            }

            // ── 01 Live logs ───────────────────────────────────────────────
            XSection { title: "Live logs"; num: "01"; width: parent.width }

            XLogList {
                id: logWidget
                width: parent.width
                height: 200

                Component.onCompleted: {
                    // Populate from buffer
                    const buf = Logs.bufferedLines()
                    for (let i = 0; i < buf.length; ++i)
                        appendLine(buf[i].text, buf[i].kind)
                }

                Connections {
                    target: Logs
                    function onLineAdded(text, kind) { logWidget.appendLine(text, kind) }
                }
            }

            // ── 02 Diagnostics ─────────────────────────────────────────────
            XSection { title: "Diagnostics"; num: "02"; width: parent.width }

            Column {
                width: parent.width
                spacing: 0

                XRow {
                    label: "Copy debug bundle"
                    desc: "config.toml + last 1000 log lines + corpus stats"
                    width: parent.width
                    XButton {
                        label: "Copy bundle"
                        onClicked: {
                            bundleToast.show()
                        }
                    }
                }

                XRow {
                    label: "Reset all settings"
                    desc: "Backs up current config, restores defaults"
                    width: parent.width
                    XButton {
                        label: "Reset all"
                        variant: "default"
                        onClicked: hardResetConfirm.visible = true
                    }
                }

                XRow {
                    label: "Report a bug"
                    isLast: true
                    width: parent.width
                    XButton {
                        label: "Open GitHub Issues"
                        variant: "ghost"
                        onClicked: Qt.openUrlExternally("https://github.com/K-Sugar/XType/issues/new")
                    }
                }
            }

            Item { height: 8 }
        }
    }

    // ── Bundle toast ───────────────────────────────────────────────────────
    Rectangle {
        id: bundleToast
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 20 }
        width: bundleText.implicitWidth + 28; height: 36; radius: 8
        color: Qt.rgba(0.08, 0.06, 0.13, 0.95); border.color: Theme.ink12; border.width: 1
        opacity: 0; z: 50
        function show() { bundleAnim.start() }
        Text { id: bundleText; anchors.centerIn: parent; text: "Bundle saved to /tmp/xtype-bundle.tar.gz"; font.family: Theme.monoFamily; font.pixelSize: 11; color: Theme.ink80; renderType: Text.NativeRendering }
        SequentialAnimation { id: bundleAnim
            NumberAnimation { target: bundleToast; property: "opacity"; to: 1; duration: 180 }
            PauseAnimation { duration: 3000 }
            NumberAnimation { target: bundleToast; property: "opacity"; to: 0; duration: 300 }
        }
    }

    // ── Hard reset confirm overlay ─────────────────────────────────────────
    Rectangle {
        id: hardResetConfirm
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        visible: false
        z: 100

        MouseArea { anchors.fill: parent }

        Rectangle {
            anchors.centerIn: parent
            width: 360; height: 180
            radius: Theme.radius.card
            color: Qt.rgba(0.08, 0.06, 0.13, 0.97)
            border.color: Qt.rgba(1, 0.37, 0.37, 0.4); border.width: 1

            Column {
                anchors { fill: parent; margins: 20 }
                spacing: 14

                Text {
                    text: "Reset all settings?"
                    font.family: Theme.sansFamily; font.pixelSize: 13; font.weight: Font.SemiBold
                    color: "#ff8a8a"
                    renderType: Text.NativeRendering
                }
                Text {
                    width: parent.width
                    text: "Your current config will be backed up to config.toml.bak.\nAll values will return to factory defaults."
                    font.family: Theme.sansFamily; font.pixelSize: 12
                    color: Theme.ink65; wrapMode: Text.WordWrap; lineHeight: 1.5
                    renderType: Text.NativeRendering
                }
                Row {
                    spacing: 8
                    XButton { label: "Cancel"; onClicked: hardResetConfirm.visible = false }
                    XButton {
                        label: "Reset"
                        variant: "primary"
                        onClicked: {
                            hardResetConfirm.visible = false
                            Config.resetAll()
                        }
                    }
                }
            }
        }
    }
}
