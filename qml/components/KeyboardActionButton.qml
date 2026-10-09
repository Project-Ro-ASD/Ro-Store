import QtQuick
import QtQuick.Controls

// Qt Quick Controls Button activates with Space, but Return/Enter handling
// is not consistently provided by the selected native style. Support both
// keys explicitly for focused buttons, retaining the built-in Space action.
Button {
    id: control

    activeFocusOnTab: true
    Keys.priority: Keys.BeforeItem

    Keys.onReturnPressed: function(event) {
        if (control.enabled && control.visible)
            control.clicked()
        event.accepted = true
    }

    Keys.onEnterPressed: function(event) {
        if (control.enabled && control.visible)
            control.clicked()
        event.accepted = true
    }
}
