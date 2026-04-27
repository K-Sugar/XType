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

        // Grain overlay — on top of all content
        XGrain { anchors.fill: parent }
    }
}
