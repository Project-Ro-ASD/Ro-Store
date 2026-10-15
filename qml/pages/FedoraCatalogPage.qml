import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import RoStore 1.0
import "../components"

Item {
    id: page
    signal backRequested()
    signal downloadsRequested()
    signal packageSelected(string name, string summary, string version, string repository)

    property var packageTransactionManager: null
    readonly property bool narrow: width < Math.max(760, 760 * metrics.fontScale)
    readonly property int margin: Math.max(12, Math.min(36, width / 16))
    ThemeMetrics { id: metrics }
    SystemPalette { id: palette }
    FedoraPackageModel { id: fedoraPackages }
    Component.onCompleted: fedoraPackages.search("")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: page.margin
        spacing: metrics.spaceMedium

        RowLayout {
            Layout.fillWidth: true
            ThemedIconButton {
                text: "Geri"
                themedIconName: "go-previous"
                onClicked: page.backRequested()
            }
            Label {
                text: "Fedora RPM Kataloğu"
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontPageTitle
                font.bold: true
                color: palette.windowText
            }
            ThemedIconButton {
                text: "Yüklemeler"
                themedIconName: "folder-download"
                onClicked: page.downloadsRequested()
            }
        }

        Label {
            text: "Fedora'nın etkin RPM depolarında uygulama ve paket ara."
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: palette.windowText
            font.family: metrics.systemFont.family
            font.pixelSize: metrics.fontSmall
        }

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Paket adı (ör. firefox, gimp, kate)"
                font.family: metrics.systemFont.family
                onAccepted: fedoraPackages.search(text)
            }
            ThemedActionButton {
                text: "Ara"
                accented: true
                Layout.minimumWidth: Math.max(94, metrics.fontPx(80))
                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                enabled: !fedoraPackages.loading
                onClicked: fedoraPackages.search(searchField.text)
            }
            ThemedIconButton {
                text: "Yenile"
                themedIconName: "view-refresh"
                Layout.minimumWidth: Math.max(116, metrics.fontPx(110))
                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                enabled: !fedoraPackages.loading
                onClicked: fedoraPackages.refresh()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Kaynak:"
                color: palette.text
                font.pixelSize: metrics.fontSmall
            }
            ComboBox {
                id: sourceBox
                Layout.fillWidth: true
                model: fedoraPackages.sources
                onActivated: fedoraPackages.sourceFilter = currentText
                Connections {
                    target: fedoraPackages
                    function onSourceFilterChanged() {
                        sourceBox.currentIndex = Math.max(0, sourceBox.find(fedoraPackages.sourceFilter))
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: fedoraPackages.loading || fedoraPackages.error.length > 0
            text: fedoraPackages.loading ? "DNF5 etkin depoları tarıyor..." : fedoraPackages.error
            wrapMode: Text.WordWrap
            color: palette.text
            font.pixelSize: metrics.fontBody
        }

        Label {
            Layout.fillWidth: true
            text: fedoraPackages.totalResults + " paket"
                  + (fedoraPackages.truncated ? " (ilk 3000 sonuç; aramayı daralt)" : "")
            color: palette.windowText
            font.pixelSize: metrics.fontSmall
        }

        ListView {
            id: packagesView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: metrics.spaceNormal
            model: fedoraPackages
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: packageCard
                required property string packageName
                required property string version
                required property string architecture
                required property string repository
                required property string summary
                required property string displayName
                required property string displaySummary
                required property string appCategory
                required property bool hasAppStream

                width: packagesView.width - 14
                height: Math.max(104, rowContent.implicitHeight + metrics.spaceMedium * 2)
                radius: metrics.radiusPanel
                color: palette.base
                border.color: packageCard.activeFocus ? palette.highlight : palette.mid
                border.width: packageCard.activeFocus ? 2 : 1
                activeFocusOnTab: true

                function openDetails() {
                    page.packageSelected(packageName, displaySummary, version, repository)
                }

                Keys.onReturnPressed: function(event) {
                    packageCard.openDetails()
                    event.accepted = true
                }
                Keys.onEnterPressed: function(event) {
                    packageCard.openDetails()
                    event.accepted = true
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: packageCard.openDetails()
                }

                ColumnLayout {
                    id: rowContent
                    anchors.fill: parent
                    anchors.margins: metrics.spaceMedium
                    spacing: metrics.spaceCompact

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: packageCard.displayName
                            color: palette.text
                            font.pixelSize: metrics.fontTitle
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        ThemedActionButton {
                            text: "Detaylar"
                            Layout.minimumWidth: Math.max(118, metrics.fontPx(105))
                            Layout.preferredHeight: Math.max(38, metrics.fontPx(38))
                            onClicked: packageCard.openDetails()
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: packageCard.displaySummary
                        color: palette.text
                        elide: Text.ElideRight
                        font.pixelSize: metrics.fontBody
                    }
                    Label {
                        Layout.fillWidth: true
                        text: (packageCard.hasAppStream ? "AppStream · " : "")
                              + repository + " · " + architecture + " · " + version
                        color: palette.text
                        opacity: 0.75
                        elide: Text.ElideRight
                        font.pixelSize: metrics.fontSmall
                    }
                }
            }

            footer: Column {
                width: packagesView.width
                spacing: metrics.spaceMedium
                Label {
                    width: parent.width
                    visible: !fedoraPackages.loading && fedoraPackages.error.length === 0
                             && fedoraPackages.totalResults === 0
                    text: "Paket bulunamadı. Farklı bir paket adı deneyin."
                    color: palette.text
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
                ThemedActionButton {
                    visible: fedoraPackages.hasMore
                    text: "Daha fazla göster"
                    width: Math.min(parent.width, 260)
                    anchors.horizontalCenter: parent.horizontalCenter
                    onClicked: fedoraPackages.loadMore()
                }
            }
        }
    }
}
