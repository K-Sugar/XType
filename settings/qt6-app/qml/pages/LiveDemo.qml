pragma ComponentBehavior: Bound
import QtQuick
import XType.Settings 1.0

Item {
    id: root
    implicitHeight: 80
    property bool paused: false

    // Four fixture sequences — plain text, no corpus data
    readonly property var demoSequences: [
        { prefix: "I wanted to reach out and ", suffix: "let you know the meeting has been rescheduled." },
        { prefix: "Thanks for your patience — ", suffix: "we'll have an update for you shortly." },
        { prefix: "Please find attached the ", suffix: "revised proposal for your review." },
        { prefix: "Looking forward to ", suffix: "connecting with you next week." },
    ]

    property int  _seqIdx:   0
    property int  _charIdx:  0
    property bool _typing:   true
    property string _display: ""
    property bool _caretOn: true

    function _advance() {
        if (root.paused) return
        const seq = root.demoSequences[root._seqIdx]
        if (root._typing) {
            if (root._charIdx < seq.suffix.length) {
                root._display = seq.prefix + seq.suffix.slice(0, root._charIdx + 1)
                root._charIdx++
                typeTimer.interval = 55 + Math.random() * 45
            } else {
                typeTimer.interval = 1800
                root._typing = false
                root._charIdx = 0
            }
        } else {
            // erase
            const full = seq.prefix + seq.suffix
            if (root._display.length > seq.prefix.length) {
                root._display = root._display.slice(0, root._display.length - 1)
                typeTimer.interval = 22
            } else {
                root._seqIdx = (root._seqIdx + 1) % root.demoSequences.length
                root._display = root.demoSequences[root._seqIdx].prefix
                root._typing = true
                typeTimer.interval = 300
            }
        }
    }

    Component.onCompleted: {
        _display = demoSequences[0].prefix
        typeTimer.start()
    }

    Timer {
        id: typeTimer
        interval: 55
        repeat: true
        running: !root.paused
        onTriggered: root._advance()
    }

    Timer {
        id: caretTimer
        interval: 530
        repeat: true
        running: true
        onTriggered: root._caretOn = !root._caretOn
    }

    Row {
        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
        spacing: 0

        Text {
            text: root._display
            font.family: Theme.monoFamily
            font.pixelSize: 13
            color: Theme.ink80
            renderType: Text.NativeRendering
        }

        // Ghost-text suggestion (static after prefix, representing AI suggestion)
        Text {
            text: root._typing ? root.demoSequences[root._seqIdx].suffix.slice(root._charIdx) : ""
            font.family: Theme.monoFamily
            font.pixelSize: 13
            color: Theme.ink25
            renderType: Text.NativeRendering
        }

        // Caret
        Rectangle {
            width: 2
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.purpleSoft
            opacity: root._caretOn ? 0.9 : 0.0
            Behavior on opacity { NumberAnimation { duration: 120 } }
        }
    }
}
