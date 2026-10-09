import QtQuick
import org.kde.kirigami.platform as Platform

// Shared measurements for custom QML surfaces.
// Standard Buttons, TextFields, ComboBoxes and ScrollBars remain styled by
// the active Qt Quick Controls / KDE application style.
//
// Kirigami's cornerRadius is a shared KDE geometry unit, NOT a reader for
// arbitrary third-party Plasma SVG radii (including Ro-Theme SVG files).
QtObject {
    id: metrics

    readonly property real radiusUnit: Platform.Units.cornerRadius
    readonly property real radiusBadge: radiusUnit * 2
    readonly property real radiusControl: radiusUnit * 2.5
    readonly property real radiusInner: radiusUnit * 3
    readonly property real radiusPanel: radiusUnit * 3.5
    readonly property real radiusCard: radiusUnit * 4.5

    readonly property real spaceUnit: Platform.Units.smallSpacing
    readonly property real spaceCompact: Platform.Units.smallSpacing
    readonly property real spaceNormal: Platform.Units.largeSpacing
    readonly property real spaceMedium: Platform.Units.largeSpacing * 1.5
    readonly property real spaceLarge: Platform.Units.largeSpacing * 2
    readonly property real spaceExtraLarge: Platform.Units.gridUnit

    readonly property int durationShort: Platform.Units.shortDuration
}
