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

    // A single size contract for the GridView cell and its AppCard.
    // Account for the 54 px icon row, 42 px summary, 1 px divider,
    // 20 px version row and 34 px action row, plus theme-derived gaps.
    readonly property real applicationCardHeight: Math.ceil(
        54 + 42 + 1 + 20 + 34
        + spaceLarge * 2
        + spaceMedium * 5
        + spaceNormal)

    readonly property int durationShort: Platform.Units.shortDuration
}
