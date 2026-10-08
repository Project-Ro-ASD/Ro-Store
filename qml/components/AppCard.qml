import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import RoStore 1.0

Rectangle {
    id: card

    SystemPalette {
        id: themePalette
    }

    // System-controlled readable secondary labels (not QPalette.mid).
    readonly property color secondaryText: Qt.rgba(
        themePalette.text.r, themePalette.text.g, themePalette.text.b, 0.76)
    readonly property color secondaryWindowText: Qt.rgba(
        themePalette.windowText.r, themePalette.windowText.g, themePalette.windowText.b, 0.76)
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
    radius: 22
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
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                width: 54
                height: 54
                radius: 16
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
                spacing: 4
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
                    radius: 9
                    color: themePalette.button
                    height: 25
                    width: categoryLabel.implicitWidth + 18

                    Label {
                        id: categoryLabel
                        anchors.centerIn: parent
                        text: card.categoryText
                        color: themePalette.highlight
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
                color: cardPackageStatus.installed ? themePalette.highlight : themePalette.text
                font.pixelSize: 13
                font.bold: true
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                radius: 12
                color: detailMouse.containsMouse ? themePalette.alternateBase : themePalette.button
                border.color: card.secondaryText
                border.width: 1
                antialiasing: true

                Label {
                    anchors.centerIn: parent
                    text: "Detayları Gör"
                    color: themePalette.buttonText
                    font.pixelSize: 12
                }

                MouseArea {
                    id: detailMouse
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
            }

            Rectangle {
                visible: cardPackageStatus.installed
                Layout.preferredWidth: 92
                Layout.preferredHeight: 34
                radius: 12
                color: themePalette.highlight
                border.color: themePalette.highlight
                border.width: 1
                antialiasing: true

                Label {
                    anchors.centerIn: parent
                    text: "Çalıştır"
                    color: themePalette.highlightedText
                    font.pixelSize: 12
                    font.bold: true
                }

                MouseArea {
                    id: runMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        cardLauncher.launch(card.packageText)
                    }
                }
            }
        }
    }
}
