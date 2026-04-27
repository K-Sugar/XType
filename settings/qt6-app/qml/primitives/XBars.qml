import QtQuick
import XType.Settings 1.0

Item {
    id: root
    property var values: [60, 45, 70, 55, 80, 65, 90]

    implicitHeight: 40
    implicitWidth: 80

    readonly property real _maxVal: {
        let m = 1
        for (let i = 0; i < values.length; ++i)
            if (values[i] > m) m = values[i]
        return m
    }

    Repeater {
        id: rep
        model: root.values

        delegate: Rectangle {
            required property real modelData
            required property int index

            readonly property int _count: root.values.length
            readonly property real _gap: 4
            readonly property real _barW: (_count > 0) ? (root.width - _gap * (_count - 1)) / _count : 8

            x: index * (_barW + _gap)
            height: Math.max(2, (modelData / root._maxVal) * root.height)
            width: _barW
            y: root.height - height
            radius: 2
            color: index === _count - 1 ? Theme.purple : Theme.ink12

            Behavior on height {
                NumberAnimation { duration: 500; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.4, 0, 0.2, 1, 1, 1] }
            }
        }
    }
}
