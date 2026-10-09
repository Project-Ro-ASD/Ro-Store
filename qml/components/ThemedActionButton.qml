import QtQuick
import QtQuick.Controls
import "."

// Reusable action button which follows the active Qt/KDE system palette.
// "accented" marks the primary action; "outlined" keeps destructive
// shortcuts neutral until the user confirms the operation.
KeyboardActionButton {
    id: control

    property bool accented: false
    property bool outlined: false

    hoverEnabled: true

    ThemeMetrics { id: metrics }
    SystemPalette { id: themePalette }

    readonly property color accentForeground: metrics.readableText(
        themePalette.highlight, themePalette.highlightedText, themePalette.text)

    readonly property color surfaceColor: accented
        ? themePalette.highlight
        : outlined
          ? themePalette.base
          : themePalette.button

    readonly property color foregroundColor: accented
        ? accentForeground
        : outlined
          ? themePalette.text
          : themePalette.buttonText

    background: Rectangle {
        radius: metrics.radiusControl

        color: control.down
            ? Qt.tint(control.surfaceColor, Qt.rgba(
                themePalette.highlight.r,
                themePalette.highlight.g,
                themePalette.highlight.b,
                0.24))
            : control.hovered
              ? Qt.tint(control.surfaceColor, Qt.rgba(
                  themePalette.highlight.r,
                  themePalette.highlight.g,
                  themePalette.highlight.b,
                  0.11))
              : control.surfaceColor

        border.width: control.activeFocus ? 3 : (control.outlined ? 1.5 : 1)
        border.color: control.activeFocus
            ? (control.accented ? control.accentForeground : themePalette.highlight)
            : control.accented
              ? themePalette.highlight
              : themePalette.mid

        opacity: control.enabled ? 1 : 0.52
        antialiasing: true

        Behavior on color {
            ColorAnimation { duration: metrics.durationShort }
        }
    }

    contentItem: Label {
        text: control.text
        font: control.font
        color: control.foregroundColor
        opacity: control.enabled ? 1 : 0.65
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
