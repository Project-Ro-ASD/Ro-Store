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
    SystemUpdateCache { id: updateCache }

    function formatCacheBytes(bytes) {
        if (bytes < 1048576)
            return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1073741824)
            return (bytes / 1048576).toFixed(1) + " MB"
        return (bytes / 1073741824).toFixed(2) + " GB"
    }

    Dialog {
        id: cacheClearConfirm
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(page.width - 24, 440)
        modal: true
        title: "RPM indirme önbelleğini temizle"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: updateCache.clear()
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: updateCache.rpmCount + " RPM dosyası silinecek. " +
                  "Bu dosyalar daha sonra yeniden indirilmek zorunda kalabilir. " +
                  "Sistem paketleri ve Fedora'nın DNF önbelleği etkilenmez."
        }
    }

    Dialog {
        id: downloadConfirm
        property var packageIds: []
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(page.width - 24, 460)
        modal: true
        title: "Seçilen RPM dosyalarını indir"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: updateCache.downloadSelected(packageIds)
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: downloadConfirm.packageIds.length + " RPM adayı indirilecek. " +
                  "Bu yalnızca indirme testidir; paket kurulmaz veya sistem güncellenmez. " +
                  "İndirilen dosyalar doğrulanır, indirmeyi iptal edebilirsin. " +
                  "İlk test en fazla 3 paket içindir."
        }
    }

    Component.onCompleted: updates.refresh()

    // The page itself scrolls independently of the packages ListView and
    // the plan's text ScrollView, so controls remain reachable on short windows.
    Flickable {
        id: pageScroll
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: pageContent.implicitHeight + page.margin * 2
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        ColumnLayout {
            id: pageContent
            x: page.margin
            y: page.margin
            width: Math.max(0, pageScroll.width - page.margin * 2 - 12)
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

        // Compact buttons: selection actions next to each other,
        // dependency preview on the far right. Preserve readable text
        // on narrow windows by placing the preview on a second row.
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
                font.pixelSize: metrics.fontSmall
                wrapMode: Text.WordWrap
            }

            RowLayout {
                id: wideActions
                Layout.fillWidth: true
                visible: pageContent.width >= Math.max(515, metrics.fontPx(515))
                spacing: metrics.spaceCompact

                Item { Layout.fillWidth: true }

                ThemedActionButton {
                    text: "Tümünü seç"
                    Layout.preferredWidth: Math.max(112, implicitWidth + 16)
                    Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                    font.pixelSize: metrics.fontSmall
                    enabled: !updates.loading && updates.updateCount > 0
                    onClicked: updates.selectAll()
                }

                ThemedActionButton {
                    text: "Seçimi temizle"
                    Layout.preferredWidth: Math.max(134, implicitWidth + 16)
                    Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                    font.pixelSize: metrics.fontSmall
                    enabled: !updates.loading && updates.selectedCount > 0
                    onClicked: updates.clearSelection()
                }

                Item { Layout.preferredWidth: metrics.spaceMedium }

                ThemedActionButton {
                    text: updates.previewRunning
                          ? "Plan sorgusunu iptal et" : "Seçilenlerin planını incele"
                    Layout.preferredWidth: Math.max(214, implicitWidth + 16)
                    Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                    font.pixelSize: metrics.fontSmall
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
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: !wideActions.visible
                spacing: metrics.spaceCompact

                RowLayout {
                    Layout.fillWidth: true
                    spacing: metrics.spaceCompact

                    Item { Layout.fillWidth: true }

                    ThemedActionButton {
                        text: "Tümünü seç"
                        Layout.preferredWidth: Math.max(112, implicitWidth + 16)
                        Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                        font.pixelSize: metrics.fontSmall
                        enabled: !updates.loading && updates.updateCount > 0
                        onClicked: updates.selectAll()
                    }

                    ThemedActionButton {
                        text: "Seçimi temizle"
                        Layout.preferredWidth: Math.max(134, implicitWidth + 16)
                        Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                        font.pixelSize: metrics.fontSmall
                        enabled: !updates.loading && updates.selectedCount > 0
                        onClicked: updates.clearSelection()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }

                    ThemedActionButton {
                        text: updates.previewRunning
                              ? "Plan sorgusunu iptal et" : "Seçilenlerin planını incele"
                        Layout.preferredWidth: Math.max(214, implicitWidth + 16)
                        Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                        font.pixelSize: metrics.fontSmall
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
                }
            }

            Label {
                Layout.fillWidth: true
                visible: updates.previewRunning
                text: "DNF5 bağımlılık planı hazırlanıyor..."
                color: palette.text
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontSmall
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
                        text: "DNF5 planı · " + updates.previewSelectionCount + " paket" + (updates.previewAllUpdates ? " · Tüm sistem" : " · Seçili paketler")
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

        Frame {
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: metrics.spaceCompact

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: "Güncelleme indirme önbelleği"
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                        font.bold: true
                        color: palette.text
                        wrapMode: Text.WordWrap
                    }
                    ThemedActionButton {
                        text: "Yenile"
                        Layout.preferredHeight: Math.max(32, metrics.fontPx(32))
                        Layout.preferredWidth: Math.max(82, implicitWidth + 12)
                        font.pixelSize: metrics.fontSmall
                        onClicked: updateCache.refresh()
                    }
                    ThemedActionButton {
                        text: updateCache.downloading ? "İptal et" : "Seçilenleri indir"
                        Layout.preferredHeight: Math.max(32, metrics.fontPx(32))
                        Layout.preferredWidth: Math.max(145, implicitWidth + 12)
                        font.pixelSize: metrics.fontSmall
                        enabled: updateCache.downloading ||
                                 (!updates.loading && !updates.previewRunning
                                  && updates.selectedCount > 0 && updates.selectedCount <= 3
                                  && updates.previewText.length > 0
                                  && updates.previewError.length === 0
                                  && updates.previewSelectionCount === updates.selectedCount
                                  && !updates.previewAllUpdates)
                        onClicked: {
                            if (updateCache.downloading) {
                                updateCache.cancelDownload()
                            } else {
                                downloadConfirm.packageIds = updates.selectedNevras()
                                if (downloadConfirm.packageIds.length > 0)
                                    downloadConfirm.open()
                            }
                        }
                    }
                    ThemedActionButton {
                        text: "Temizle"
                        Layout.preferredHeight: Math.max(32, metrics.fontPx(32))
                        Layout.preferredWidth: Math.max(90, implicitWidth + 12)
                        font.pixelSize: metrics.fontSmall
                        enabled: updateCache.rpmCount > 0
                        onClicked: cacheClearConfirm.open()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: updateCache.rpmCount + " RPM · " + page.formatCacheBytes(updateCache.bytes)
                          + " · İndirme yalnızca onayla başlar; kurulum yapılmaz."
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSmall
                    color: palette.text
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: !updateCache.downloading && updates.selectedCount > 0
                             && !(updates.previewText.length > 0
                                  && updates.previewError.length === 0
                                  && updates.previewSelectionCount === updates.selectedCount
                                  && !updates.previewAllUpdates
                                  && updates.selectedCount <= 3)
                    text: updates.selectedCount > 3
                          ? "İlk indirme testi için en fazla 3 paket seçin."
                          : "İndirme için önce seçili paketlerin planını inceleyin ve sorgunun bitmesini bekleyin."
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSmall
                    color: palette.text
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: updateCache.downloadStatus.length > 0
                    text: updateCache.downloadStatus
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSmall
                    color: palette.text
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    text: updateCache.path
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSmall
                    color: palette.text
                    elide: Text.ElideMiddle
                }
                Label {
                    Layout.fillWidth: true
                    visible: updateCache.error.length > 0
                    text: updateCache.error
                    color: palette.text
                    wrapMode: Text.WordWrap
                }
            }
        }

        ListView {
            id: updateList
            Layout.fillWidth: true
            // Give the nested update list its own independent vertical range.
            // Without a preferred height it would collapse inside the outer Flickable.
            Layout.preferredHeight: Math.max(250, Math.min(400, page.height * 0.58))
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
}
