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
    height: 218
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
        anchors.margins: 16
        spacing: metrics.spaceMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceMedium

            Rectangle {
                width: 54
                height: 54
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
                    font.pixelSize: 23
                    font.bold: true
                }
            }

            ColumnLayout {
                spacing: metrics.spaceCompact
                Layout.fillWidth: true

                Label {
                    text: card.titleText
                    color: themePalette.text
                    font.pixelSize: 18
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Rectangle {
                    radius: metrics.radiusBadge
                    color: themePalette.button
                    border.color: themePalette.mid
                    border.width: 1
                    height: 25
                    width: categoryLabel.implicitWidth + 18

                    Label {
                        id: categoryLabel
                        anchors.centerIn: parent
                        text: card.categoryText
                        color: card.mutedAccent
                        font.pixelSize: 12
                    }
                }
            }
        }

        Label {
            text: card.summaryText
            color: themePalette.text
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            Layout.fillWidth: true
            Layout.preferredHeight: 42
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: themePalette.mid
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "v" + card.versionText
                color: card.secondaryText
                font.pixelSize: 13
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: cardPackageStatus.installed ? "Kurulu" : "Resmi"
                color: cardPackageStatus.installed ? card.mutedAccent : themePalette.text
                font.pixelSize: 13
                font.bold: true
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceNormal

            // Native Qt Quick Controls inherit the active KDE control style.
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                text: "Detayları Gör"
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

            Button {
                visible: cardPackageStatus.installed
                Layout.preferredWidth: 92
                Layout.preferredHeight: 34
                text: "Çalıştır"
                onClicked: cardLauncher.launch(card.packageText)
            }
        }
    }
}
