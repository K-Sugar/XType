pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import XType.Settings 1.0

Item {
    id: root
    anchors.fill: parent
    property string selectedId: ""

    Row {
        anchors.fill: parent

        // Left column — app list
        Item {
            width: parent.width * 0.5
            height: parent.height

            Flickable {
                id: leftFlick
                anchors { fill: parent; topMargin: 22; leftMargin: 28 }
                contentHeight: appCol.implicitHeight + 44
                clip: true

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle { radius: 3; color: Theme.ink12 }
                }

                Column {
                    id: appCol
                    width: leftFlick.width - 28
                    spacing: 6

                    XSection { title: "Per-app settings"; num: "01"; width: parent.width }

                    Repeater {
                        model: Engine.activeApps
                        delegate: XAppRow {
                            required property var modelData
                            appLabel: modelData.name
                            appSub: modelData.name
                            iconCls: ""
                            selected: root.selectedId === modelData.name
                            width: parent ? parent.width : 0
                            onClicked: root.selectedId = modelData.name

                            XToggle {
                                on: {
                                    const entry = Config.apps[modelData.name]
                                    return entry ? (entry.enabled !== undefined ? entry.enabled : true) : true
                                }
                                onToggled: (v) => Config.setApp(modelData.name, "enabled", v)
                            }
                        }
                    }

                    Text {
                        visible: Engine.activeApps.length === 0
                        text: "No apps seen yet. XType will show apps here as you use them."
                        font.family: Theme.sansFamily; font.pixelSize: 13
                        color: Theme.ink45
                        width: parent.width
                        wrapMode: Text.WordWrap
                        renderType: Text.NativeRendering
                    }
                }
            }
        }

        // Right column — detail card
        Item {
            width: parent.width * 0.5
            height: parent.height

            Flickable {
                id: rightFlick
                anchors { fill: parent; topMargin: 22; rightMargin: 28 }
                contentHeight: detailCard.implicitHeight + 44
                clip: true

                XCard {
                    id: detailCard
                    width: rightFlick.width
                    visible: root.selectedId !== ""

                    Column {
                        width: parent.width
                        spacing: 16

                        // Eyebrow
                        Text {
                            text: root.selectedId !== ""
                                  ? "Configuring  ·  " + root.selectedId
                                  : ""
                            font.family: Theme.monoFamily; font.pixelSize: 11
                            font.capitalization: Font.AllUppercase; font.letterSpacing: 1.2
                            color: Theme.purpleSoft
                            renderType: Text.NativeRendering
                        }

                        // Enabled toggle
                        XRow {
                            label: "Enabled in this app"
                            width: parent.width
                            isLast: false
                            XToggle {
                                on: {
                                    if (root.selectedId === "") return true
                                    const entry = Config.apps[root.selectedId]
                                    return entry ? (entry.enabled !== undefined ? entry.enabled : true) : true
                                }
                                onToggled: (v) => {
                                    if (root.selectedId !== "")
                                        Config.setApp(root.selectedId, "enabled", v)
                                }
                            }
                        }

                        // Mode selector
                        XRow {
                            label: "Mode"
                            width: parent.width
                            isLast: false
                            ComboBox {
                                model: ["Default", "Code-aware", "Email tone", "Casual", "Off"]
                                currentIndex: {
                                    if (root.selectedId === "") return 0
                                    const entry = Config.apps[root.selectedId]
                                    const m = entry ? entry.mode || "Default" : "Default"
                                    return Math.max(0, ["Default","Code-aware","Email tone","Casual","Off"].indexOf(m))
                                }
                                onActivated: (i) => {
                                    if (root.selectedId === "") return
                                    Config.setApp(root.selectedId, "mode", model[i])
                                }
                                implicitWidth: 160; implicitHeight: 32
                                font.family: Theme.sansFamily; font.pixelSize: 12

                                contentItem: Text {
                                    leftPadding: 10
                                    text: parent.displayText
                                    font: parent.font
                                    color: Theme.ink100
                                    verticalAlignment: Text.AlignVCenter
                                    renderType: Text.NativeRendering
                                }
                                background: Rectangle {
                                    radius: 7; color: parent.pressed ? Theme.ink06 : Theme.ink03
                                    border.width: 1; border.color: parent.hovered ? Theme.ink25 : Theme.ink12
                                }
                            }
                        }

                        // Suggestion length (comingSoon)
                        XRow {
                            label: "Suggestion length"
                            isLast: false
                            width: parent.width
                            XSlider {
                                min: 4; max: 48; step: 1
                                value: {
                                    if (root.selectedId === "") return Config.numPredict
                                    const entry = Config.apps[root.selectedId]
                                    return entry && entry.num_predict ? entry.num_predict : Config.numPredict
                                }
                                width: 160
                                onCommitted: (v) => {
                                    if (root.selectedId !== "")
                                        Config.setApp(root.selectedId, "num_predict", v)
                                }
                            }
                        }

                        // Model override
                        XRow {
                            label: "Model"
                            isLast: true
                            width: parent.width
                            XComboBox {
                                id: modelCombo
                                width: 200

                                model: {
                                    const installed = Ollama.availableModels
                                    const defaultLabel = "Default (" + Config.model + ")"
                                    if (root.selectedId === "") return [defaultLabel]
                                    const entry = Config.apps[root.selectedId]
                                    const saved = entry && entry.model ? entry.model : ""
                                    let list = [defaultLabel]
                                    for (let i = 0; i < installed.length; ++i) list.push(installed[i])
                                    if (saved && installed.indexOf(saved) < 0)
                                        list.push(saved + "  (not installed)")
                                    return list
                                }

                                currentIndex: {
                                    if (root.selectedId === "") return 0
                                    const installed = Ollama.availableModels
                                    const entry = Config.apps[root.selectedId]
                                    const saved = entry && entry.model ? entry.model : ""
                                    if (!saved) return 0
                                    const idx = installed.indexOf(saved)
                                    if (idx >= 0) return idx + 1
                                    return installed.length + 1  // "not installed" entry at end
                                }

                                onActivated: (i) => {
                                    if (root.selectedId === "") return
                                    if (i === 0) {
                                        Config.setApp(root.selectedId, "model", null)
                                    } else {
                                        const installed = Ollama.availableModels
                                        if (i <= installed.length)
                                            Config.setApp(root.selectedId, "model", installed[i - 1])
                                        // i > installed.length → "(not installed)" entry, already saved — no action
                                    }
                                }

                                ToolTip.text: "Ollama model name for this app.\nLeave empty to use the global default.\nSmaller: qwen2.5:1.5b (~80ms). Larger: gemma3:4b (~300ms, GPU recommended)."
                                ToolTip.visible: hovered
                            }
                        }
                    }
                }

                // Empty state
                Text {
                    anchors.centerIn: parent
                    visible: root.selectedId === ""
                    text: "Select an app from the left to configure it."
                    font.family: Theme.sansFamily; font.pixelSize: 13
                    color: Theme.ink45
                    renderType: Text.NativeRendering
                }
            }
        }
    }
}
