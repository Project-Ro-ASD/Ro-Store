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


    // Use KDE/Qt's application font instead of choosing a font family.
    // Pixel sizes in views are reference sizes (14 px default), not fixed
    // sizes: changes to the application font propagate through these bindings.
    readonly property font systemFont: Qt.application.font
    readonly property real systemFontPixels: systemFont.pixelSize > 0
        ? systemFont.pixelSize
        : (systemFont.pointSize > 0 ? systemFont.pointSize * 96 / 72 : 14)
    readonly property real fontScale: Math.max(0.75, systemFontPixels / 14)

    function fontPx(referencePixels) {
        return Math.max(10, Math.round(referencePixels * fontScale))
    }

    readonly property int fontCaption: fontPx(12)
    readonly property int fontSmall: fontPx(13)
    readonly property int fontBody: fontPx(14)
    readonly property int fontBodyLarge: fontPx(15)
    readonly property int fontTitle: fontPx(18)
    readonly property int fontSection: fontPx(19)
    readonly property int fontPageTitle: fontPx(22)

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
