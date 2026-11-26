import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import RoStore 1.0
import "../components"

Item {
    id: page
    property string source: "rpm"
    property int visibleLimit: 48
    signal backRequested()

    SystemPalette { id: palette }
    ThemeMetrics { id: metrics }
    SoftwareHistoryModel { id: history }

    Component.onCompleted: history.refresh()
    onSourceChanged: visibleLimit = 48

    ScrollView {
        id: view
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.max(0, view.availableWidth - 28)
            x: 14
            y: 14
            spacing: metrics.spaceMedium

            RowLayout {
                Layout.fillWidth: true
                ThemedIconButton {
                    text: "Geri"
                    themedIconName: "go-previous"
                    Layout.preferredHeight: 42
                    onClicked: page.backRequested()
                }
                Label {
                    Layout.fillWidth: true
                    text: "İşlem geçmişi"
                    font.pixelSize: metrics.fontPageTitle
                    font.bold: true
                    color: palette.windowText
                    elide: Text.ElideRight
                }
                ThemedIconButton {
                    text: "Yenile"
                    themedIconName: "view-refresh"
                    Layout.preferredHeight: 42
                    enabled: !history.loading
                    onClicked: history.refresh()
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Son DNF5 işlemleri ve Flatpak değişiklikleri. Flatpak geçmişi başarısız işlemleri kaydetmeyebilir."
                wrapMode: Text.WordWrap
                color: palette.windowText
            }
            Label {
                Layout.fillWidth: true
                visible: history.error.length > 0
                text: history.error
                wrapMode: Text.WordWrap
                color: palette.windowText
            }

            RowLayout {
                Layout.fillWidth: true
                ThemedActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    text: "RPM / DNF5"
                    accented: page.source === "rpm"
                    onClicked: page.source = "rpm"
                }
                ThemedActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    text: "Flatpak"
                    accented: page.source === "flatpak"
                    onClicked: page.source = "flatpak"
                }
            }

            Label {
                Layout.fillWidth: true
                visible: history.loading
                text: "Geçmiş okunuyor..."
                color: palette.windowText
            }
            Label {
                Layout.fillWidth: true
                visible: !history.loading
                         && (page.source === "rpm" ? history.rpmHistory.length
                                                  : history.flatpakHistory.length) === 0
                text: "Bu kaynakta görüntülenecek kayıt bulunamadı."
                color: palette.windowText
            }

            Repeater {
                model: (page.source === "rpm" ? history.rpmHistory
                                               : history.flatpakHistory).slice(0, page.visibleLimit)
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: details.implicitHeight + 20
                    radius: metrics.radiusCard
                    color: palette.base
                    border.color: palette.mid
                    ColumnLayout {
                        id: details
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 12
                        y: 10
                        spacing: 4
                        Label {
                            Layout.fillWidth: true
                            text: page.source === "rpm"
                                  ? "DNF5 #" + modelData.id + " · " + (modelData.status || "Durum bilinmiyor")
                                  : (modelData.application || "Flatpak kaynağı") + " · " + (modelData.change || "Değişiklik")
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: page.source === "rpm"
                                  ? (modelData.time || "") + " · " + modelData.alteredCount + " paket"
                                  : (modelData.time || "") + " · " + (modelData.scope || "")
                                    + " · " + (modelData.remote || "Kaynak bilinmiyor")
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: page.source === "rpm"
                            text: modelData.command || ""
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            ThemedActionButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                visible: (page.source === "rpm" ? history.rpmHistory.length
                                                 : history.flatpakHistory.length) > page.visibleLimit
                text: "Daha fazla göster"
                onClicked: page.visibleLimit += 48
            }
            Item { Layout.fillWidth: true; Layout.preferredHeight: 14 }
        }
    }
}
