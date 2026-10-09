import QtQuick
import RoStore 1.0
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
    // Shared font source emits an explicit change notification for every
    // application font change, including while a StackView page is inactive.
    readonly property font systemFont: SystemFontMonitor.currentFont
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

    // KDE can provide a bright accent with a light highlightedText role.
    // Evaluate sRGB contrast instead of assuming highlightedText is readable
    // against highlight in every color scheme (e.g. Fedora Breeze Light).
    function channelLuminance(value) {
        return value <= 0.04045 ? value / 12.92
                                : Math.pow((value + 0.055) / 1.055, 2.4)
    }

    function luminance(colorValue) {
        return 0.2126 * channelLuminance(colorValue.r)
             + 0.7152 * channelLuminance(colorValue.g)
             + 0.0722 * channelLuminance(colorValue.b)
    }

    function contrastRatio(foreground, background) {
        const a = luminance(foreground)
        const b = luminance(background)
        return (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05)
    }

    function readableText(background, preferred, alternate) {
        if (contrastRatio(preferred, background) >= 4.5)
            return preferred
        if (contrastRatio(alternate, background) >= 4.5)
            return alternate
        // Black or white always reaches at least 4.5:1 against opaque sRGB.
        return luminance(background) > 0.179
             ? Qt.rgba(0, 0, 0, 1)
             : Qt.rgba(1, 1, 1, 1)
    }

    // Subtly separate app and repository artwork from equal-brightness
    // surfaces in light KDE themes, while retaining dark-theme behavior.
    // These are derived from the active palette, not hardcoded colors.
    function iconTileColor(base, foreground) {
        return Qt.rgba(
            base.r * 0.88 + foreground.r * 0.12,
            base.g * 0.88 + foreground.g * 0.12,
            base.b * 0.88 + foreground.b * 0.12,
            1.0
        )
    }

    function iconTileBorderColor(mid, foreground) {
        return Qt.rgba(
            mid.r * 0.70 + foreground.r * 0.30,
            mid.g * 0.70 + foreground.g * 0.30,
            mid.b * 0.70 + foreground.b * 0.30,
            0.90
        )
    }

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

    // Keep the grid cell and card height aligned as KDE fonts change.
    readonly property real applicationCardHeaderHeight: Math.max(
        54, Math.ceil(fontTitle * 1.3 + Math.max(25, fontCaption + 10) + spaceCompact))
    readonly property real applicationCardSummaryHeight: Math.max(42, Math.ceil(fontBody * 2.8))
    readonly property real applicationCardStatusHeight: Math.max(20, Math.ceil(fontSmall * 1.5))
    readonly property real applicationCardActionHeight: Math.max(34, fontPx(34))
    readonly property real applicationCardHeight: Math.ceil(
        applicationCardHeaderHeight + applicationCardSummaryHeight + 1
        + applicationCardStatusHeight + applicationCardActionHeight
        + spaceLarge * 2 + spaceMedium * 5 + spaceNormal)

    readonly property int durationShort: Platform.Units.shortDuration
}
