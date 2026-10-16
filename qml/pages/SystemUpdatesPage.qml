import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import RoStore 1.0
import "../components"

Item {
    id: page
    signal backRequested()
    readonly property int margin: Math.max(12, Math.min(36, width / 16))

    ThemeMetrics { id: metrics }
    SystemPalette { id: palette }
    SystemUpdateModel { id: updates }

    Component.onCompleted: updates.refresh()

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
                text: "Sistem Güncellemeleri"
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: palette.windowText
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontPageTitle
                font.bold: true
            }
            ThemedIconButton {
                text: "Yeniden denetle"
                themedIconName: "view-refresh"
                enabled: !updates.loading
                onClicked: updates.refresh()
            }
        }

        Label {
            text: "Fedora'da kurulu tüm RPM paketlerinin güncelleme adaylarını etkin DNF5 depolarından denetler. Bu ekran henüz yalnızca bilgi gösterir; sistemde hiçbir paket değiştirmez."
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.family: metrics.systemFont.family
            font.pixelSize: metrics.fontBody
            color: palette.text
        }

        Label {
            Layout.fillWidth: true
            text: updates.loading ? "Sistem güncellemeleri denetleniyor..."
                  : updates.checked ? updates.updateCount + " RPM güncelleme adayı"
                  : "Henüz güncelleme denetlenmedi."
            font.family: metrics.systemFont.family
            font.pixelSize: metrics.fontTitle
            font.bold: true
            color: palette.windowText
        }

        Label {
            Layout.fillWidth: true
            visible: updates.checked
            text: "Son başarılı kontrol: " + Qt.formatDateTime(updates.lastChecked, "dd.MM.yyyy HH:mm")
            color: palette.text
            font.pixelSize: metrics.fontSmall
        }

        Label {
            Layout.fillWidth: true
            visible: updates.hasKernelUpdates
            text: "Kernel güncellemesi adayı bulundu. Kurulumdan sonra yeniden başlatma gerekebilir. Yeniden başlatma gereksinimi bu aşamada otomatik hesaplanmıyor."
            wrapMode: Text.WordWrap
            color: palette.text
            font.pixelSize: metrics.fontBody
        }

        Label {
            Layout.fillWidth: true
            visible: updates.error.length > 0
            text: updates.error
            wrapMode: Text.WordWrap
            color: palette.text
            font.pixelSize: metrics.fontBody
        }

        ListView {
            id: updateList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: metrics.spaceCompact
            model: updates
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: updateCard
                required property string packageName
                required property string updateVersion
                required property string architecture
                required property string repository
                required property bool kernelUpdate

                width: updateList.width - 14
                height: Math.max(82, cardColumn.implicitHeight + metrics.spaceMedium * 2)
                radius: metrics.radiusPanel
                color: palette.base
                border.color: palette.mid
                border.width: 1

                ColumnLayout {
                    id: cardColumn
                    anchors.fill: parent
                    anchors.margins: metrics.spaceMedium
                    spacing: metrics.spaceCompact

                    Label {
                        Layout.fillWidth: true
                        text: updateCard.packageName
                              + (updateCard.kernelUpdate ? "  ·  Kernel" : "")
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontTitle
                        font.bold: true
                        color: palette.text
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Yeni sürüm: " + updateCard.updateVersion
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                        color: palette.text
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Kaynak: " + updateCard.repository
                              + "  ·  Mimari: " + updateCard.architecture
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontSmall
                        color: palette.text
                        elide: Text.ElideRight
                    }
                }
            }

            footer: Label {
                width: updateList.width
                visible: !updates.loading && updates.checked
                         && updates.updateCount === 0 && updates.error.length === 0
                text: "Etkin depolarda kurulu RPM paketleri için güncelleme adayı bulunamadı."
                color: palette.text
                font.pixelSize: metrics.fontBody
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
