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
                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                Layout.preferredWidth: Math.max(90, implicitWidth + 16)
                font.pixelSize: metrics.fontBody
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
                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                Layout.preferredWidth: Math.max(166, implicitWidth + 16)
                font.pixelSize: metrics.fontBody
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

        // Keep the count in a separate line so the actions are never
        // squeezed by long text, especially at 400 px or increased KDE font.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceCompact

            Label {
                Layout.fillWidth: true
                text: updates.selectedCount + " paket seçili"
                      + (updates.selectedKernelCount > 0
                         ? " · " + updates.selectedKernelCount + " kernel paketi" : "")
                color: palette.text
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontBody
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: metrics.spaceMedium

                ThemedActionButton {
                    text: "Tümünü seç"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 210
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                    font.pixelSize: metrics.fontBody
                    enabled: !updates.loading && updates.updateCount > 0
                    onClicked: updates.selectAll()
                }

                ThemedActionButton {
                    text: "Seçimi temizle"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 210
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                    font.pixelSize: metrics.fontBody
                    enabled: !updates.loading && updates.selectedCount > 0
                    onClicked: updates.clearSelection()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceCompact

            ThemedActionButton {
                text: updates.previewRunning
                      ? "Plan sorgusunu iptal et" : "Seçilenlerin planını incele"
                Layout.fillWidth: true
                Layout.maximumWidth: 320
                Layout.minimumWidth: 0
                Layout.preferredHeight: Math.max(44, metrics.fontPx(44))
                font.pixelSize: metrics.fontBody
                enabled: updates.previewRunning
                         || (!updates.loading && updates.selectedCount > 0)
                onClicked: {
                    if (updates.previewRunning) {
                        updates.cancelPreview()
                    } else {
                        previewFrame.collapsed = false
                        updates.previewPlan()
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: updates.previewRunning
                text: "DNF5 bağımlılık planı hazırlanıyor..."
                color: palette.text
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontBody
                wrapMode: Text.WordWrap
            }
        }

        Label {
            visible: updates.previewError.length > 0
            Layout.fillWidth: true
            text: updates.previewError
            color: palette.text
            font.pixelSize: metrics.fontBody
            wrapMode: Text.WordWrap
        }

        Frame {
            id: previewFrame
            property bool collapsed: false
            visible: updates.previewText.length > 0
            Layout.fillWidth: true
            // Collapsing keeps the heading and toggle visible while allowing
            // the RPM list to reclaim most of the window's vertical space.
            Layout.preferredHeight: collapsed
                                    ? Math.max(52, previewHeader.implicitHeight + 20)
                                    : Math.min(300, Math.max(180, page.height * 0.42))

            ColumnLayout {
                anchors.fill: parent
                spacing: metrics.spaceCompact

                RowLayout {
                    id: previewHeader
                    Layout.fillWidth: true
                    spacing: metrics.spaceMedium

                    Label {
                        text: "DNF5 planı · Salt okunur önizleme"
                        color: palette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                        font.bold: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    ThemedActionButton {
                        text: previewFrame.collapsed ? "Büyüt" : "Küçült"
                        Layout.preferredWidth: Math.max(100, implicitWidth + 16)
                        Layout.preferredHeight: Math.max(38, metrics.fontPx(38))
                        font.pixelSize: metrics.fontBody
                        Accessible.name: previewFrame.collapsed
                                         ? "Güncelleme planını büyüt"
                                         : "Güncelleme planını küçült"
                        onClicked: previewFrame.collapsed = !previewFrame.collapsed
                    }
                }

                ScrollView {
                    visible: !previewFrame.collapsed
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    TextArea {
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.NoWrap
                        text: updates.previewText
                        font.family: "monospace"
                        font.pixelSize: metrics.fontSmall
                    }
                }
            }
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
                required property string installedVersion
                required property string architecture
                required property string repository
                required property bool kernelUpdate
                required property bool updateSelected

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

                    RowLayout {
                        Layout.fillWidth: true
                        CheckBox {
                            checked: updateCard.updateSelected
                            onToggled: updates.setSelected(updateCard.packageName,
                                                           updateCard.architecture, checked)
                            Accessible.name: updateCard.packageName + " güncellemesini seç"
                        }
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
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Kurulu: " + (updateCard.installedVersion.length > 0
                              ? updateCard.installedVersion : "Bilinmiyor")
                              + "  →  Yeni: " + updateCard.updateVersion
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
