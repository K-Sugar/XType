pragma Singleton
import QtQuick
import XType.Settings 1.0

Item {
    id: reloadCenter

    property bool dirty: false
    property bool inflight: false
    property bool lastReloadOk: true

    Connections {
        target: Config
        function onSaved() { reloadCenter.dirty = true }
    }

    Connections {
        target: Reloader
        function onReloadStarted() { reloadCenter.inflight = true }
        function onReloadFinished(ok) {
            reloadCenter.inflight = false
            reloadCenter.lastReloadOk = ok
            if (ok) reloadCenter.dirty = false
        }
    }
}
