import QtQuick
import QtQuick.Window
import XType.Settings 1.0

Window {
    id: window
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "XType Settings"

    property string currentPage: "general"

    Rectangle {
        id: windowBg
        anchors.fill: parent
        radius: Theme.radius.window
        color: Qt.rgba(0.08, 0.06, 0.13, Theme.bgAlpha)
        border.color: Qt.rgba(0.655, 0.545, 0.980, 0.18)
        border.width: 1

        // Radial accent — top-left corner glow
        Rectangle {
            anchors { top: parent.top; left: parent.left }
            width: parent.width * 0.55; height: parent.height
            radius: parent.radius
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(0.518, 0.431, 0.918, 0.10) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        XTitleBar {
            id: titlebar
            anchors { top: parent.top; left: parent.left; right: parent.right }
            onMinimizeRequested: window.showMinimized()
            onMaximizeRequested: window.showMaximized()
            onCloseRequested: window.close()
        }

        // App body: sidebar + content area
        Item {
            anchors { top: titlebar.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }

            XSidebar {
                id: sidebar
                anchors { top: parent.top; left: parent.left; bottom: parent.bottom }
                currentPage: window.currentPage
                onPageChanged: (id) => window.currentPage = id
            }

            // Content placeholder — Step 5+ fills this with StackView pages
            Rectangle {
                anchors { top: parent.top; left: sidebar.right; right: parent.right; bottom: parent.bottom }
                color: "transparent"

                Text {
                    anchors.centerIn: parent
                    text: window.currentPage
                    color: Theme.ink45
                    font.family: Theme.sansFamily
                    font.pixelSize: 14
                    renderType: Text.NativeRendering
                }
            }
        }

        // Reload banner — slides up from the bottom when dirty
        Rectangle {
            id: reloadBanner
            anchors { left: parent.left; right: parent.right; leftMargin: 1; rightMargin: 1 }
            height: 44
            radius: 0
            color: ReloadCenter.lastReloadOk ? Qt.rgba(0.518, 0.431, 0.918, 0.18)
                                             : Qt.rgba(1.0, 0.37, 0.37, 0.18)
            border.width: 1
            border.color: ReloadCenter.lastReloadOk ? Qt.rgba(0.655, 0.545, 0.980, 0.30)
                                                    : Qt.rgba(1.0, 0.37, 0.37, 0.40)
            y: ReloadCenter.dirty ? parent.height - height - 1 : parent.height
            Behavior on y { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
            clip: true

            Row {
                anchors.centerIn: parent
                spacing: 12

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: ReloadCenter.inflight       ? "Reloading fcitx5…"
                        : ReloadCenter.lastReloadOk   ? "Settings saved — click to reload fcitx5"
                                                      : "Reload failed — check logs"
                    font.family: Theme.sansFamily; font.pixelSize: 12
                    color: ReloadCenter.lastReloadOk ? Theme.purpleSoft : "#ff8a8a"
                    renderType: Text.NativeRendering
                }

                Rectangle {
                    visible: !ReloadCenter.inflight
                    anchors.verticalCenter: parent.verticalCenter
                    width: retryLabel.implicitWidth + 20; height: 26; radius: 5
                    color: Qt.rgba(0.518, 0.431, 0.918, 0.18)
                    border.width: 1; border.color: Qt.rgba(0.655, 0.545, 0.980, 0.30)

                    Text {
                        id: retryLabel
                        anchors.centerIn: parent
                        text: ReloadCenter.lastReloadOk ? "Reload" : "Retry"
                        font.family: Theme.sansFamily; font.pixelSize: 11
                        color: Theme.purpleSoft
                        renderType: Text.NativeRendering
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Reloader.reload()
                    }
                }
            }
        }

        // Grain overlay — on top of all content
        XGrain { anchors.fill: parent }
    }
}
