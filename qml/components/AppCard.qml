import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."
import RoStore 1.0

Rectangle {
    id: card

    ThemeMetrics { id: metrics }

    SystemPalette {
        id: themePalette
    }

    // System-controlled readable secondary labels (not QPalette.mid).
    readonly property color secondaryText: Qt.rgba(
        themePalette.text.r, themePalette.text.g, themePalette.text.b, 0.76)
    readonly property color secondaryWindowText: Qt.rgba(
        themePalette.windowText.r, themePalette.windowText.g, themePalette.windowText.b, 0.76)

    // Blend the active accent with the foreground to keep meaningful labels
    // colored but comfortable in both light and dark color schemes.
    readonly property color mutedAccent: Qt.rgba(
        themePalette.highlight.r * 0.6 + themePalette.text.r * 0.4,
        themePalette.highlight.g * 0.6 + themePalette.text.g * 0.4,
        themePalette.highlight.b * 0.6 + themePalette.text.b * 0.4,
        1.0)
    property string titleText: ""
    property string summaryText: ""
    property string categoryText: ""
    property string versionText: ""
    property string packageText: ""
    property string descriptionText: ""
    property string iconUrl: ""

    property var packageTransactionManager: null
    property int statusRefreshSerial: 0

    signal detailRequested(
        string title,
        string summary,
        string description,
        string category,
        string version,
        string packageName,
        string iconUrl
    )

    width: 300
    height: metrics.applicationCardHeight
    radius: metrics.radiusCard
    color: mouseArea.containsMouse ? themePalette.alternateBase : themePalette.base
    border.color: mouseArea.containsMouse ? themePalette.highlight : themePalette.mid
    border.width: 1
    antialiasing: true
    clip: false

    PackageStatus {
        id: cardPackageStatus
    }

    AppLauncher {
        id: cardLauncher
    }

    Component.onCompleted: {
        cardPackageStatus.checkInstalled(card.packageText, card.versionText)
    }

    Connections {
        target: card.packageTransactionManager

        function onPackageStateChanged(packageName) {
            if (packageName !== card.packageText)
                return

            cardPackageStatus.checkInstalled(
                card.packageText,
                card.versionText
            )
        }
    }

    onPackageTextChanged: {
        cardPackageStatus.checkInstalled(card.packageText, card.versionText)
    }

    onVersionTextChanged: {
        cardPackageStatus.checkInstalled(card.packageText, card.versionText)
    }

    onStatusRefreshSerialChanged: {
        console.log(
            "CARD PACKAGE STATUS REFRESH:",
            card.packageText,
            statusRefreshSerial
        )

        cardPackageStatus.checkInstalled(
            card.packageText,
            card.versionText
        )
    }

    Behavior on color {
        ColorAnimation { duration: 140 }
    }

    Behavior on border.color {
        ColorAnimation { duration: 140 }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            card.detailRequested(
                card.titleText,
                card.summaryText,
                card.descriptionText,
                card.categoryText,
                card.versionText,
                card.packageText,
                card.iconUrl
            )
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: metrics.spaceLarge
        spacing: metrics.spaceMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceMedium

            Rectangle {
                // Qt Quick Layouts position/size their children using Layout
                // hints. Plain width/height are overridden by RowLayout.
                Layout.preferredWidth: 54
                Layout.preferredHeight: 54
                Layout.minimumWidth: 54
                Layout.minimumHeight: 54
                radius: metrics.radiusInner
                color: themePalette.button
                clip: true
                antialiasing: true

                Image {
                    id: appIcon
                    anchors.centerIn: parent
                    width: 36
                    height: 36
                    source: card.iconUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    visible: card.iconUrl.length > 0 && status === Image.Ready
                }

                Label {
                    anchors.centerIn: parent
                    visible: !appIcon.visible
                    text: card.titleText.length > 0 ? card.titleText[0].toUpperCase() : "R"
                    color: themePalette.buttonText
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontPx(23)
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: metrics.spaceCompact
                Layout.fillWidth: true

                Label {
                    text: card.titleText
                    color: themePalette.text
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontTitle
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Rectangle {
                    // ColumnLayout ignores manually assigned width/height
                    // once it controls the child. Keep the badge's actual
                    // layout size bound to the live system-font text metrics.
                    Layout.preferredHeight: Math.max(25, categoryLabel.implicitHeight + 8)
                    Layout.preferredWidth: categoryLabel.implicitWidth + 18
                    Layout.minimumHeight: Math.max(25, categoryLabel.implicitHeight + 8)
                    Layout.fillWidth: false
                    radius: metrics.radiusBadge
                    color: themePalette.button
                    border.color: themePalette.mid
                    border.width: 1
                    clip: true

                    Label {
                        id: categoryLabel
                        anchors.fill: parent
                        anchors.leftMargin: 9
                        anchors.rightMargin: 9
                        text: card.categoryText
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        color: card.mutedAccent
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontCaption
                    }
                }
            }
        }

        Label {
            text: card.summaryText
            color: themePalette.text
            font.family: metrics.systemFont.family
            font.pixelSize: metrics.fontBody
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            Layout.fillWidth: true
            Layout.preferredHeight: metrics.applicationCardSummaryHeight
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.minimumHeight: 1
            color: themePalette.mid
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "v" + card.versionText
                color: card.secondaryText
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontSmall
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: cardPackageStatus.installed ? "Kurulu" : "Resmi"
                color: cardPackageStatus.installed ? card.mutedAccent : themePalette.text
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontSmall
                font.bold: true
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceNormal

            // Use palette roles rather than fixed colors: inverted neutral
            // action like the original design, and system accent for Launch.
            KeyboardActionButton {
                id: detailsButton
                Layout.fillWidth: true
                Layout.preferredHeight: metrics.applicationCardActionHeight
                hoverEnabled: true
                text: "Detayları Gör"
                // Keep the two custom action labels in sync with the card
                // labels when Plasma's font changes without restarting.
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontBody

                background: Rectangle {
                    radius: metrics.radiusControl
                    color: detailsButton.down
                           ? Qt.tint(themePalette.text, Qt.rgba(themePalette.base.r,
                                                                themePalette.base.g,
                                                                themePalette.base.b, 0.22))
                           : detailsButton.hovered
                             ? Qt.tint(themePalette.text, Qt.rgba(themePalette.base.r,
                                                                  themePalette.base.g,
                                                                  themePalette.base.b, 0.12))
                             : themePalette.text
                    // The white details button needs a blue focus outline.
                    border.width: detailsButton.activeFocus ? 3 : 0
                    border.color: themePalette.highlight
                    antialiasing: true

                    Behavior on color {
                        ColorAnimation { duration: metrics.durationShort }
                    }
                }

                contentItem: Label {
                    text: detailsButton.text
                    color: themePalette.base
                    font: detailsButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: card.detailRequested(
                    card.titleText,
                    card.summaryText,
                    card.descriptionText,
                    card.categoryText,
                    card.versionText,
                    card.packageText,
                    card.iconUrl
                )
            }

            KeyboardActionButton {
                id: launchButton
                visible: cardPackageStatus.installed
                Layout.preferredWidth: Math.max(92, metrics.fontPx(92))
                Layout.preferredHeight: metrics.applicationCardActionHeight
                hoverEnabled: true
                text: "Çalıştır"
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontBody

                background: Rectangle {
                    radius: metrics.radiusControl
                    color: launchButton.down
                           ? Qt.tint(themePalette.highlight, Qt.rgba(themePalette.base.r,
                                                                     themePalette.base.g,
                                                                     themePalette.base.b, 0.22))
                           : launchButton.hovered
                             ? Qt.tint(themePalette.highlight, Qt.rgba(themePalette.base.r,
                                                                       themePalette.base.g,
                                                                       themePalette.base.b, 0.12))
                             : themePalette.highlight
                    border.width: launchButton.activeFocus ? 3 : 0
                    border.color: themePalette.highlightedText
                    antialiasing: true

                    Behavior on color {
                        ColorAnimation { duration: metrics.durationShort }
                    }
                }

                contentItem: Label {
                    text: launchButton.text
                    color: themePalette.highlightedText
                    font: launchButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: cardLauncher.launch(card.packageText)
            }
        }
    }
}
