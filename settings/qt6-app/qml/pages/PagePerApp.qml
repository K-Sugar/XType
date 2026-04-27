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
                        model: AppsKnown.knownApps()
                        delegate: XAppRow {
                            required property var modelData
                            appLabel: modelData.label
                            appSub: modelData.canonical
                            iconCls: modelData.iconCls
                            selected: root.selectedId === modelData.id
                            width: parent ? parent.width : 0
                            onClicked: root.selectedId = modelData.id

                            XToggle {
                                on: {
                                    const entry = Config.apps[modelData.canonical]
                                    return entry ? (entry.enabled !== undefined ? entry.enabled : true) : true
                                }
                                onToggled: (v) => Config.setApp(modelData.canonical, "enabled", v)
                            }
                        }
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
                                  ? "Configuring  ·  " + AppsKnown.labelForCanonical(AppsKnown.canonicalForId(root.selectedId))
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
                                    const c = AppsKnown.canonicalForId(root.selectedId)
                                    const entry = Config.apps[c]
                                    return entry ? (entry.enabled !== undefined ? entry.enabled : true) : true
                                }
                                onToggled: (v) => {
                                    if (root.selectedId !== "")
                                        Config.setApp(AppsKnown.canonicalForId(root.selectedId), "enabled", v)
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
                                    const entry = Config.apps[AppsKnown.canonicalForId(root.selectedId)]
                                    const m = entry ? entry.mode || "Default" : "Default"
                                    return Math.max(0, ["Default","Code-aware","Email tone","Casual","Off"].indexOf(m))
                                }
                                onActivated: (i) => {
                                    if (root.selectedId === "") return
                                    Config.setApp(AppsKnown.canonicalForId(root.selectedId), "mode", model[i])
                                }
                                implicitWidth: 160; implicitHeight: 32
                                font.family: Theme.sansFamily; font.pixelSize: 12

                                ToolTip.text: "Per-app mode — engine still uses global prompt (Session 20)"
                                ToolTip.visible: currentIndex !== 0 && currentIndex !== 4 && hovered

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
                            isLast: true
                            width: parent.width
                            XSlider {
                                min: 4; max: 48; step: 1
                                value: {
                                    if (root.selectedId === "") return Config.numPredict
                                    const entry = Config.apps[AppsKnown.canonicalForId(root.selectedId)]
                                    return entry && entry.num_predict ? entry.num_predict : Config.numPredict
                                }
                                comingSoon: true
                                width: 160
                                onCommitted: (v) => {
                                    if (root.selectedId !== "")
                                        Config.setApp(AppsKnown.canonicalForId(root.selectedId), "num_predict", v)
                                }

                                ToolTip.text: "Per-app override — engine still uses global value (Session 20)"
                                ToolTip.visible: comingSoon && hovered
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
