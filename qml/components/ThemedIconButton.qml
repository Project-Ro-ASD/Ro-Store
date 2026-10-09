import QtQuick
import QtQuick.Controls
import RoStore 1.0

// Icon theme updates do not always invalidate icons cached by existing
// Qt Quick Controls buttons. Clear and restore the icon name on the next
// event-loop tick; keep the Button itself and its click handlers intact.
Button {
    id: control

    property string themedIconName: ""
    property bool iconRefreshPending: false

    icon.name: iconRefreshPending ? "" : themedIconName
    icon.cache: false

    Timer {
        id: restoreIconTimer
        interval: 0
        repeat: false
        onTriggered: control.iconRefreshPending = false
    }

    Connections {
        target: SystemIconMonitor

        function onRevisionChanged() {
            control.iconRefreshPending = true
            restoreIconTimer.restart()
        }
    }
}
