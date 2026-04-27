import QtQuick
import QtQuick.Controls.Basic
import XType.Settings 1.0

Item {
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

            // ── 01 Engine ──────────────────────────────────────────────────
            XSection { title: "Engine"; num: "01"; width: parent.width }

            Column {
                width: parent.width
                spacing: 0
                XRow {
                    label: "Enable XType globally"
                    desc: "Master on/off for ghost-text suggestions"
                    width: parent.width
                    XToggle {
                        on: Config.engineEnabled
                        onToggled: (v) => Config.engineEnabled = v
                    }
                }
                XRow {
                    label: "Trigger"
                    desc: "When to start the suggestion request"
                    width: parent.width
                    XSegmented {
                        options: ["Pause", "Manual"]
                        comingSoonIndices: [1]
                        selectedIndex: Config.triggerMode === "manual" ? 1 : 0
                        onSelected: (i) => Config.triggerMode = (i === 1 ? "manual" : "pause")
                    }
                }
                XRow {
                    label: "Trigger delay"
                    desc: "Wait after keystroke before requesting"
                    width: parent.width
                    XSlider {
                        min: 100; max: 800; step: 25
                        value: Config.debounceMs
                        onCommitted: (v) => Config.debounceMs = v
                        width: 200
                        // Custom label formatter (override internal Text with a binding)
                        Component.onCompleted: {}
                    }
                }
                XRow {
                    label: "Max suggestion length"
                    isLast: true
                    width: parent.width
                    XSlider {
                        min: 4; max: 48; step: 1
                        value: Config.numPredict
                        onCommitted: (v) => Config.numPredict = v
                        width: 200
                    }
                }
            }

            // ── 02 Acceptance ──────────────────────────────────────────────
            XSection { title: "Acceptance"; num: "02"; width: parent.width }

            Column {
                width: parent.width
                spacing: 0
                XRow {
                    label: "Accept key"
                    desc: "Key that accepts the full suggestion"
                    width: parent.width
                    Row {
                        spacing: 6
                        XPill {
                            label: "Tab"
                            on: Config.acceptKey === "tab"
                            onClicked: Config.acceptKey = "tab"
                        }
                        XPill {
                            label: "Enter"
                            on: Config.acceptKey === "enter"
                            comingSoon: true
                            onClicked: Config.acceptKey = "enter"
                        }
                        XPill {
                            label: "→"
                            on: Config.acceptKey === "right"
                            comingSoon: true
                            onClicked: Config.acceptKey = "right"
                        }
                    }
                }
                XRow {
                    label: "Partial accept (word-by-word)"
                    desc: "Accept one word at a time"
                    width: parent.width
                    XToggle {
                        on: Config.partialAccept
                        onToggled: (v) => Config.partialAccept = v
                    }
                }
                XRow {
                    label: "Dismiss on Esc"
                    isLast: true
                    width: parent.width
                    XToggle {
                        on: Config.escDismisses
                        onToggled: (v) => Config.escDismisses = v
                    }
                }
            }

            // ── Live preview ───────────────────────────────────────────────
            XSection { title: "Live preview"; width: parent.width }

            XCard {
                width: parent.width
                height: demo.implicitHeight + 32

                Text {
                    x: 0; y: 0
                    text: "Ghost text typewriter"
                    font.family: Theme.sansFamily; font.pixelSize: 11
                    font.capitalization: Font.AllUppercase; font.letterSpacing: 1.2
                    color: Theme.ink45
                    renderType: Text.NativeRendering
                }

                LiveDemo {
                    id: demo
                    anchors { top: parent.top; topMargin: 22; left: parent.left; right: parent.right }
                    paused: !Config.engineEnabled
                }
            }

            Item { height: 8 }
        }
    }
}
