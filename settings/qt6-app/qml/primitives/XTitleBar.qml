import QtQuick
import QtQuick.Window
import XType.Settings 1.0

Item {
    id: titlebar
    implicitHeight: 36

    signal minimizeRequested
    signal maximizeRequested
    signal closeRequested

    // Subtle top gradient
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0.518, 0.431, 0.918, 0.06) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Drag area — sits under the win-controls Row in z
    MouseArea {
        anchors.fill: parent
        onPressed: (mouse) => {
            if (mouse.button === Qt.LeftButton)
                Window.window.startSystemMove()
        }
    }

    // Left: engine dot + state text
    Row {
        anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
        spacing: 8

        XEngineDot { active: true; anchors.verticalCenter: parent.verticalCenter }

        Text {
            text: "ready"
            font.family: Theme.monoFamily
            font.pixelSize: 11
            color: Theme.ink45
            renderType: Text.NativeRendering
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // Center: title (pointer-events none via no MouseArea)
    Text {
        anchors.centerIn: parent
        text: "XType — Settings"
        font.family: Theme.sansFamily
        font.pixelSize: 13
        font.weight: Font.Medium
        color: Theme.ink80
        renderType: Text.NativeRendering
    }

    // Right: window controls — declared after drag area so they sit above it
    Row {
        anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
        spacing: 2

        WinBtn { btnIcon: "min"; onClicked: titlebar.minimizeRequested() }
        WinBtn { btnIcon: "max"; onClicked: titlebar.maximizeRequested() }
        WinBtn { btnIcon: "close"; onClicked: titlebar.closeRequested() }
    }

    // Bottom border
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.ink06
    }

    // ── Inline component ────────────────────────────────────────────────────
    component WinBtn: Rectangle {
        id: btnRoot
        property string btnIcon: "min"
        signal clicked

        width: 28; height: 24; radius: 4
        property bool hovered: btnMa.containsMouse
        readonly property bool isClose: btnIcon === "close"

        color: hovered ? (isClose ? "#c44534" : Theme.ink06) : "transparent"
        Behavior on color { ColorAnimation { duration: 140 } }

        MouseArea {
            id: btnMa
            anchors.fill: parent
            hoverEnabled: true
            onClicked: btnRoot.clicked()
        }

        Canvas {
            id: btnCanvas
            anchors.centerIn: parent
            width: 10; height: 10

            property color stroke: btnRoot.hovered
                ? (btnRoot.isClose ? "#ffffff" : Theme.ink100)
                : Theme.ink65
            onStrokeChanged: requestPaint()

            onPaint: {
                const c = getContext("2d")
                c.clearRect(0, 0, 10, 10)
                c.strokeStyle = stroke
                c.lineCap = "round"
                if (btnRoot.btnIcon === "min") {
                    c.lineWidth = 1.4
                    c.beginPath(); c.moveTo(1.5, 5); c.lineTo(8.5, 5); c.stroke()
                } else if (btnRoot.btnIcon === "max") {
                    c.lineWidth = 1.2
                    c.beginPath(); c.rect(1.5, 1.5, 7, 7); c.stroke()
                } else {
                    c.lineWidth = 1.4
                    c.beginPath(); c.moveTo(2, 2); c.lineTo(8, 8); c.stroke()
                    c.beginPath(); c.moveTo(8, 2); c.lineTo(2, 8); c.stroke()
                }
            }
        }
    }
}
