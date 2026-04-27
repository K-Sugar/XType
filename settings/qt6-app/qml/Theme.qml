pragma Singleton
import QtQuick

// Generated colors from scripts/gen-theme-colors.py — re-run when hue changes.
QtObject {
    id: theme

    readonly property real  hue:          288
    readonly property real  bgAlpha:      0.78
    readonly property real  grain:        0.18
    readonly property string sansFamily:  "Inter"
    readonly property string monoFamily:  "JetBrains Mono"

    // OKLCH(0.62 0.17 288) → sRGB
    readonly property color purple:      "#8470e5"
    // OKLCH(0.78 0.12 288)
    readonly property color purpleSoft:  "#b4abff"
    // OKLCH(0.42 0.15 288)
    readonly property color purpleDeep:  "#4d3898"
    // OKLCH(0.62 0.18 288) — used at alpha 0.20 / 0.10
    readonly property color purpleGlow:  Qt.rgba(0.518, 0.431, 0.918, 0.20)
    readonly property color purpleTint:  Qt.rgba(0.518, 0.431, 0.918, 0.10)

    // Ink: base #f0ecfa with varying alpha (matches prototype.css --ink-*)
    readonly property color ink100:      "#f0ecfa"
    readonly property color ink80:       Qt.rgba(0.941, 0.925, 0.980, 0.82)
    readonly property color ink65:       Qt.rgba(0.941, 0.925, 0.980, 0.65)
    readonly property color ink45:       Qt.rgba(0.941, 0.925, 0.980, 0.42)
    readonly property color ink25:       Qt.rgba(0.941, 0.925, 0.980, 0.22)
    readonly property color ink12:       Qt.rgba(0.941, 0.925, 0.980, 0.12)
    readonly property color ink06:       Qt.rgba(0.941, 0.925, 0.980, 0.06)
    readonly property color ink03:       Qt.rgba(0.941, 0.925, 0.980, 0.03)

    readonly property var spacing: ({ row: 12, section: 28, page: 22 })
    readonly property var radius:  ({ window: 14, card: 10, pill: 999, btn: 7 })
    readonly property var ease:    ({ standard: [0.4,0,0.2,1], bouncy: [0.34,1.4,0.6,1] })
    readonly property var fontSize:({ row: 13.5, label: 11.5, mono: 12 })
}
