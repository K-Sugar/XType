pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import XType.Settings 1.0

Item {
    id: root
    anchors.fill: parent

    // Model-switch confirmation overlay
    property string _pendingModel: ""

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

            // ── 01 Active model ────────────────────────────────────────────
            XSection { title: "Active model"; num: "01"; width: parent.width }

            XCard {
                accent: true
                width: parent.width

                Column {
                    width: parent.width
                    spacing: 12

                    Text {
                        text: Config.model
                        font.family: Theme.monoFamily; font.pixelSize: 15; font.weight: Font.DemiBold
                        color: Theme.ink100
                        renderType: Text.NativeRendering
                    }

                    Text {
                        readonly property var _meta: Ollama.activeMeta(Config.model)
                        text: _meta.family + "  ·  " + _meta.quant + "  ·  ~" + Math.round(_meta.size_mb / 1024 * 10) / 10 + " GB"
                        font.family: Theme.sansFamily; font.pixelSize: 11
                        color: Theme.ink65
                        renderType: Text.NativeRendering
                    }

                    // Stat grid
                    Row {
                        width: parent.width
                        spacing: 12

                        Repeater {
                            model: [
                                { label: "Latency", value: Engine.latencyP50 + " ms" },
                                { label: "RAM",     value: Engine.ramGb.toFixed(1) + " GB" },
                                { label: "CPU",     value: Engine.cpuPct + "%" },
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                width: (parent.width - 24) / 3; height: 52
                                radius: 7
                                color: Theme.ink03; border.width: 1; border.color: Theme.ink06

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 3
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.value
                                        font.family: Theme.monoFamily; font.pixelSize: 14; font.weight: Font.DemiBold
                                        color: Theme.ink100
                                        renderType: Text.NativeRendering
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.label
                                        font.family: Theme.sansFamily; font.pixelSize: 10
                                        font.capitalization: Font.AllUppercase; font.letterSpacing: 1.2
                                        color: Theme.ink45
                                        renderType: Text.NativeRendering
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── 02 Inference settings ──────────────────────────────────────
            XSection { title: "Inference settings"; num: "02"; width: parent.width }

            Column {
                width: parent.width
                spacing: 0
                XRow {
                    label: "Quantisation"
                    desc: "Model weight precision"
                    width: parent.width
                    XSegmented {
                        options: ["F16", "Q8", "Q4", "Q3"]
                        comingSoon: true
                        selectedIndex: 2
                        width: 180
                    }
                }
                XRow {
                    label: "Context window"
                    desc: "Tokens of context fed to the model"
                    width: parent.width
                    XSlider {
                        min: 512; max: 4096; step: 256
                        value: Config.contextWindow
                        onCommitted: (v) => Config.contextWindow = v
                        width: 200
                    }
                }
                XRow {
                    label: "Threads"
                    desc: "CPU inference threads (unset = Ollama default)"
                    isLast: true
                    width: parent.width
                    XSlider {
                        min: 1; max: 12; step: 1
                        value: Config.threads.valid ? Config.threads : 0
                        formatFn: function(v) { return (v === null || v === undefined || v === 0) ? "auto" : v + " threads" }
                        width: 200
                        onCommitted: (v) => Config.threads = v
                    }
                }
            }

            // ── 03 Available models ────────────────────────────────────────
            XSection { title: "Available models"; num: "03"; width: parent.width }

            Column {
                width: parent.width
                spacing: 6

                Repeater {
                    model: {
                        const all = Ollama.availableModels
                        return all.filter(m => m !== Config.model)
                    }
                    delegate: XAppRow {
                        required property string modelData
                        appLabel: modelData
                        appSub: {
                            const m = Ollama.activeMeta(modelData)
                            return m.family + "  ·  " + m.quant
                        }
                        iconCls: ""
                        width: parent ? parent.width : 0
                        onClicked: root._pendingModel = modelData
                    }
                }

                // Import GGUF row
                XAppRow {
                    appLabel: "+ Import GGUF model"
                    appSub: "Use ollama pull <model> from a terminal"
                    iconCls: ""
                    width: parent.width
                    onClicked: ggufToast.show()
                }

                // Empty state
                Text {
                    visible: Ollama.availableModels.length === 0
                    text: "Ollama not detected — install with\n`ollama serve` or run `ollama pull qwen2.5:1.5b`"
                    font.family: Theme.monoFamily; font.pixelSize: 12
                    color: Theme.ink45
                    lineHeight: 1.6
                    renderType: Text.NativeRendering
                }
            }

            Item { height: 8 }
        }
    }

    // ── Toast ──────────────────────────────────────────────────────────────
    Rectangle {
        id: ggufToast
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 20 }
        width: toastText.implicitWidth + 28; height: 36; radius: 8
        color: Qt.rgba(0.08, 0.06, 0.13, 0.95)
        border.color: Theme.ink12; border.width: 1
        opacity: 0
        z: 50

        function show() { showAnim.start() }

        Text {
            id: toastText
            anchors.centerIn: parent
            text: "Use `ollama pull <model>` from a terminal"
            font.family: Theme.monoFamily; font.pixelSize: 12
            color: Theme.ink80
            renderType: Text.NativeRendering
        }

        SequentialAnimation {
            id: showAnim
            NumberAnimation { target: ggufToast; property: "opacity"; to: 1; duration: 180 }
            PauseAnimation { duration: 3000 }
            NumberAnimation { target: ggufToast; property: "opacity"; to: 0; duration: 300 }
        }
    }

    // ── Model switch confirmation overlay ──────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        visible: root._pendingModel !== ""
        z: 100

        MouseArea { anchors.fill: parent }

        Rectangle {
            anchors.centerIn: parent
            width: 340; height: 160
            radius: Theme.radius.card
            color: Qt.rgba(0.08, 0.06, 0.13, 0.97)
            border.color: Theme.ink12; border.width: 1

            Column {
                anchors { fill: parent; margins: 20 }
                spacing: 16

                Text {
                    width: parent.width
                    text: "Switch to " + root._pendingModel + "?"
                    font.family: Theme.sansFamily; font.pixelSize: 13; font.weight: Font.DemiBold
                    color: Theme.ink100; wrapMode: Text.WordWrap
                    renderType: Text.NativeRendering
                }

                Text {
                    width: parent.width
                    text: "XType will pause for ~5 seconds while the new model loads."
                    font.family: Theme.sansFamily; font.pixelSize: 12
                    color: Theme.ink65; wrapMode: Text.WordWrap
                    renderType: Text.NativeRendering
                }

                Row {
                    spacing: 8
                    XButton {
                        label: "Cancel"
                        onClicked: root._pendingModel = ""
                    }
                    XButton {
                        label: "Switch"
                        variant: "primary"
                        onClicked: {
                            Config.model = root._pendingModel
                            root._pendingModel = ""
                            Reloader.reload()
                        }
                    }
                }
            }
        }
    }
}
