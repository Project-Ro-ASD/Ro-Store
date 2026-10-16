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
    property var packageTransactionManager: null
    SystemUpdateCache { id: updateCache; packageTransactionManager: page.packageTransactionManager }
    SystemOfflineUpgrade { id: offlineUpdate; transactionManager: page.packageTransactionManager }
    property bool showAdvancedTools: false
    Connections {
        target: updates
        function onSelectionChanged() {
            offlineUpdate.invalidateIfSelectionChanged(updates.selectedNevras())
        }
        function onUpdatesChanged() {
            offlineUpdate.invalidateIfSelectionChanged(updates.selectedNevras())
        }
    }

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
        onAccepted: updateCache.downloadSelected(packageIds, updates.selectedDownloadBytes, updates.selectedUnknownSizes)
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: downloadConfirm.packageIds.length + " RPM seçili; " +
                  updateCache.countCached(downloadConfirm.packageIds) + " doğrulanmış RPM önbellekte, " +
                  (downloadConfirm.packageIds.length -
                   updateCache.countCached(downloadConfirm.packageIds)) + " RPM eksik. " +
                  "Tahmini seçili paket toplamı: " +
                  page.formatCacheBytes(updates.selectedDownloadBytes) +
                  " (bağımlılıklar ve depo verisi hariç; gerçek trafik farklı olabilir). " +
                  "Bu yalnızca indirme testidir; hiçbir RPM kurulmayacak. " +
                  "Sınırlı internet kullanıyorsan toplamı incelemeden onaylama."
        }
    }

    Dialog {
        id: offlinePrepareConfirmation
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(page.width - 24, 530)
        modal: true
        title: "Sistem güncellemesini çevrimdışı hazırla"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: offlineUpdate.stageOffline()
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "DNF5 tarafından çözülen TEK işlem planı: " +
                  offlineUpdate.planCount + " işlem; " +
                  offlineUpdate.removalCount + " paket kaldırma; " +
                  (offlineUpdate.kernelAffected ? "KERNEL DEĞİŞİKLİĞİ VAR. " : "") +
                  "Yaklaşık " + page.formatCacheBytes(offlineUpdate.planBytes) +
                  " indirme. " +
                  "Paket kaldırmaları dahil bütün DNF5 planını incelemiş olmalısın. " +
                  "İndirilen paketler çevrimdışı uygulama için hazırlanacak. " +
                  "Şimdi RPM KURULMAZ ve bilgisayar YENİDEN BAŞLATILMAZ. " +
                  "İndirme miktarı Ro-Store'un bağımsız RPM önbelleğinden düşülmemiştir. " +
                  "Hazırlamayı onaylıyor musun?"
        }
    }

    Dialog {
        id: recoveryConfirm
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(page.width - 24, 520)
        title: "Çevrimdışı işlem kurtarma kaydını temizle"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: offlineUpdate.acknowledgeRecovery()
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Sistem yeniden başlatıldı ve DNF5 bekleyen işlem işareti bulunamadı. " +
                  "Bu, güncellemenin başarılı olduğunu KANITLAMAZ. " +
                  "dnf5 offline log ve dnf5 history çıktısını kontrol ettiyseniz " +
                  "yalnızca Ro-Store kurtarma kilidini kaldırmayı onaylayabilirsiniz. " +
                  "Bu işlem geçmişini geri almaz veya herhangi bir RPM kurmaz."
        }
    }

    Dialog {
        id: offlineRebootConfirmation
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(page.width - 24, 540)
        title: "Yeniden başlat ve çevrimdışı güncellemeyi uygula"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: offlineUpdate.requestConfirmedReboot()
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "UYARI: Bu işlem yönetici onayı isteyebilir ve bilgisayarı hemen " +
                  "yeniden başlatarak DNF5'in hazırladığı bütün RPM değişikliklerini " +
                  "çevrimdışı uygulamaya başlayabilir. Bütün çalışmalarını kaydet, " +
                  "güç ve internet bağlantını kontrol et. İşlem sırasında zorla " +
                  "kapatma yapma. Yeniden başlatmayı şimdi başlatmak istiyor musun?"
        }
    }

    Dialog {
        id: discardOfflineConfirmation
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(page.width - 24, 530)
        title: "Hazırlanmış çevrimdışı güncellemeyi iptal et"
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: offlineUpdate.discardPreparedOffline()
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "DNF5 tarafından önceden hazırlanmış sistem güncellemesi " +
                  "ve bu işleme ait indirilen DNF5 paket dosyaları temizlenecek. " +
                  "Yeniden indirme gerekebilir. Ro-Store'un bağımsız RPM önbelleği " +
                  "ve şu anda kurulu paketler etkilenmez. " +
                  "Yalnızca temizleme sonucu DNF5 tarafından doğrulanırsa " +
                  "güvenlik kaydı kaldırılır. İptal etmeyi onaylıyor musun?"
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
            text: "Fedora için kullanılabilir güncellemeleri buradan kontrol edebilir, paketleri seçip güncelleme planını inceleyebilirsiniz. Güncellemeler yalnızca onayınızla hazırlanır ve ayrı bir yeniden başlatma onayından sonra uygulanır."
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

        RowLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceCompact
            ThemedActionButton {
                text: "Tümünü seç"
                Layout.preferredHeight: Math.max(36, metrics.fontPx(36))
                enabled: !updates.loading && updates.updateCount > 0
                onClicked: updates.selectAll()
            }
            ThemedActionButton {
                text: "Seçimi kaldır"
                Layout.preferredHeight: Math.max(36, metrics.fontPx(36))
                enabled: !updates.loading && updates.selectedCount > 0
                onClicked: updates.clearSelection()
            }
            Item { Layout.fillWidth: true }
        }
        ThemedActionButton {
            text: page.showAdvancedTools ? "Gelişmiş seçenekleri gizle" : "Gelişmiş seçenekler"
            Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
            Layout.alignment: Qt.AlignRight
            font.pixelSize: metrics.fontSmall
            onClicked: page.showAdvancedTools = !page.showAdvancedTools
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
            Label {
                Layout.fillWidth: true
                visible: updates.selectedCount > 0
                text: "Seçili paketlerin tahmini indirme boyutu: "
                      + page.formatCacheBytes(updates.selectedDownloadBytes)
                      + (updates.selectedUnknownSizes > 0
                          ? " · " + updates.selectedUnknownSizes
                            + " paket boyutu bilinmiyor; gerçek DNF5 işlem toplamı farklı olabilir."
                          : " · Bağımlılıkların indirme boyutu dahil değildir.")
                color: palette.text
                font.pixelSize: metrics.fontSmall
                wrapMode: Text.WordWrap
            }

            RowLayout {
                id: wideActions
                visible: page.showAdvancedTools && pageContent.width >= Math.max(515, metrics.fontPx(515))
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
                visible: page.showAdvancedTools && !wideActions.visible
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
                visible: page.showAdvancedTools && updates.previewRunning
                text: "DNF5 bağımlılık planı hazırlanıyor..."
                color: palette.text
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontSmall
                wrapMode: Text.WordWrap
            }
        }

        Label {
            visible: page.showAdvancedTools && updates.previewError.length > 0
            Layout.fillWidth: true
            text: updates.previewError
            color: palette.text
            font.pixelSize: metrics.fontBody
            wrapMode: Text.WordWrap
        }

        Frame {
            id: previewFrame
            visible: page.showAdvancedTools && updates.previewText.length > 0
            property bool collapsed: false
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
                Label {
                    Layout.fillWidth: true
                    text: "Güncellemeleri hazırla"
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontBody
                    font.bold: true
                    color: palette.text
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    text: "1. Paketleri seçin. 2. Güncelleme planını oluşturup eklenecek ve kaldırılacak paketleri inceleyin. " +
                          "3. Onay vererek dosyaları hazırlayın. Güncellemeler hemen kurulmaz; yeniden başlatma ayrıca onaylanır."
                    font.pixelSize: metrics.fontSmall
                    color: palette.text
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    ThemedActionButton {
                        text: offlineUpdate.busy ? "Plan hazırlanıyor..." : "Güncelleme planını oluştur"
                        Layout.preferredWidth: Math.max(210, implicitWidth + 12)
                        Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                        font.pixelSize: metrics.fontSmall
                        enabled: !offlineUpdate.busy && !offlineUpdate.outcomeUnknown
                                 && !offlineUpdate.offlineReady
                                 && !updateCache.downloading
                                 && !updates.loading && !updates.previewRunning
                                 && updates.selectedCount > 0
                                 && updateCache.transactionAvailable
                        onClicked: offlineUpdate.resolve(updates.selectedNevras())
                    }
                    ThemedActionButton {
                        text: "Planı iptal et"
                        Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                        Layout.preferredWidth: Math.max(96, implicitWidth + 12)
                        font.pixelSize: metrics.fontSmall
                        enabled: offlineUpdate.ready && !offlineUpdate.preparing
                        onClicked: offlineUpdate.abandonPlan()
                    }
                    Item { Layout.fillWidth: true }
                }
                Label {
                    Layout.fillWidth: true
                    visible: offlineUpdate.message.length > 0
                    text: offlineUpdate.message
                    font.pixelSize: metrics.fontSmall
                    color: palette.text
                    wrapMode: Text.WordWrap
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 175
                    visible: offlineUpdate.ready || offlineUpdate.preparing || offlineUpdate.offlineReady
                    TextArea {
                        text: offlineUpdate.planText
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.NoWrap
                        font.family: "monospace"
                        font.pixelSize: metrics.fontSmall
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: offlineUpdate.preparing
                    spacing: metrics.spaceCompact
                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: offlineUpdate.progressPercent
                        indeterminate: offlineUpdate.planBytes === 0
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "DNF5 indirilen veri: " +
                              page.formatCacheBytes(offlineUpdate.downloadedBytes) +
                              " / tahmini " + page.formatCacheBytes(offlineUpdate.planBytes) +
                              " (%" + offlineUpdate.progressPercent + "). " +
                              "Gerçek aktarılan miktar DNF5'in bildirdiği değerlere dayanır."
                        font.pixelSize: metrics.fontSmall
                        color: palette.text
                        wrapMode: Text.WordWrap
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: offlineUpdate.ready || offlineUpdate.preparing
                    ThemedActionButton {
                        text: "Güncelleme dosyalarını hazırla"
                        Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                        Layout.preferredWidth: Math.max(230, implicitWidth + 12)
                        font.pixelSize: metrics.fontSmall
                        enabled: offlineUpdate.ready && !offlineUpdate.preparing
                        onClicked: offlinePrepareConfirmation.open()
                    }
                    ThemedActionButton {
                        text: "İptal iste"
                        Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                        Layout.preferredWidth: Math.max(100, implicitWidth + 12)
                        font.pixelSize: metrics.fontSmall
                        enabled: offlineUpdate.preparing
                        onClicked: offlineUpdate.requestCancellation()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: offlineUpdate.outcomeUnknown
                    ThemedActionButton {
                        text: "Kurtarma durumunu denetle"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                        font.pixelSize: metrics.fontSmall
                        onClicked: offlineUpdate.recheckRecovery()
                    }
                    ThemedActionButton {
                        text: "Kaydı onayla ve kilidi kaldır"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(34, metrics.fontPx(34))
                        font.pixelSize: metrics.fontSmall
                        enabled: offlineUpdate.recoveryVerified
                        onClicked: recoveryConfirm.open()
                    }
                }
                ThemedActionButton {
                    visible: offlineUpdate.offlineReady
                    text: offlineUpdate.clearingPrepared
                          ? "Çevrimdışı hazırlık temizleniyor..."
                          : "Hazırlanmış güncellemeyi iptal et"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(38, metrics.fontPx(38))
                    font.pixelSize: metrics.fontSmall
                    enabled: !offlineUpdate.clearingPrepared
                             && !offlineUpdate.rebootRequestRunning
                             && !offlineUpdate.outcomeUnknown
                    onClicked: discardOfflineConfirmation.open()
                }
                ThemedActionButton {
                    visible: offlineUpdate.offlineReady
                    text: offlineUpdate.rebootRequestRunning
                          ? "Yeniden başlatma isteği yürütülüyor..."
                          : "Yeniden başlat ve güncellemeleri uygula"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(38, metrics.fontPx(38))
                    font.pixelSize: metrics.fontSmall
                    enabled: offlineUpdate.offlineReady
                             && !offlineUpdate.rebootRequestRunning
                             && !offlineUpdate.outcomeUnknown
                             && !offlineUpdate.clearingPrepared
                    onClicked: offlineRebootConfirmation.open()
                }
                Label {
                    Layout.fillWidth: true
                    visible: offlineUpdate.offlineReady
                    text: "Çevrimdışı hazırlama kaydı oluşturuldu. Şimdi kurulum yapılmadı. " +
                          "Yeniden başlatma, geçmiş ve kurtarma kontrolü ayrı onay gerektirir; " +
                          "DNF5 durumunu doğrulamadan bir kez daha hazırlama başlatmayın. " +
                          "Hazırlanan işlemi ayrıca başlatmak isterseniz terminalde " +
                          "sudo dnf5 offline reboot komutu kullanılır. " +
                          "Bu komut bilgisayarı yeniden başlatıp güncelleme işlemini uygulamaya başlayabilir; " +
                          "kullanmadan önce çalışmalarınızı kaydedin."
                    font.pixelSize: metrics.fontSmall
                    font.bold: true
                    color: palette.text
                    wrapMode: Text.WordWrap
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
                        text: "İndirme önbelleği (isteğe bağlı test aracı)"
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                        font.bold: true
                        color: palette.text
                        wrapMode: Text.WordWrap
                    }
                    ThemedActionButton {
                        text: "Dosyaları kontrol et"
                        Layout.preferredHeight: Math.max(32, metrics.fontPx(32))
                        Layout.preferredWidth: Math.max(82, implicitWidth + 12)
                        font.pixelSize: metrics.fontSmall
                        onClicked: updateCache.refresh()
                    }
                    ThemedActionButton {
                        text: updateCache.downloading ? "İndirmeyi durdur" : "Yalnızca RPM dosyalarını indir"
                        Layout.preferredHeight: Math.max(32, metrics.fontPx(32))
                        Layout.preferredWidth: Math.max(145, implicitWidth + 12)
                        font.pixelSize: metrics.fontSmall
                        enabled: updateCache.downloading ||
                                 (updateCache.transactionAvailable && !updates.loading && !updates.previewRunning
                                  && updates.selectedCount > 0 && updates.selectedCount <= updateCache.maximumBatch
                                  && updates.previewText.length > 0
                                  && updates.previewError.length === 0
                                  && updates.previewSelectionCount === updates.selectedCount
                                  && !updates.previewAllUpdates
                                  && updates.selectedUnknownSizes === 0
                                  && updates.selectedDownloadBytes <= 52428800)
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
                        text: "İndirilen dosyaları sil"
                        Layout.preferredHeight: Math.max(32, metrics.fontPx(32))
                        Layout.preferredWidth: Math.max(90, implicitWidth + 12)
                        font.pixelSize: metrics.fontSmall
                        enabled: updateCache.rpmCount > 0
                        onClicked: cacheClearConfirm.open()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: updates.selectedCount > 0 && updates.selectedCount <= updateCache.maximumBatch
                    text: "Seçimdeki doğrulanmış RPM: " +
                          updateCache.countCached(updates.selectedNevras()) + " / " +
                          updates.selectedCount + " · Eksik: " +
                          (updates.selectedCount - updateCache.countCached(updates.selectedNevras()))
                    font.pixelSize: metrics.fontSmall
                    color: palette.text
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: updates.selectedCount > 0
                             && (updates.selectedUnknownSizes > 0
                                 || updates.selectedDownloadBytes > 52428800)
                    text: updates.selectedUnknownSizes > 0
                          ? "Bazı seçili RPM dosyalarının boyutu bilinmiyor; güvenli indirme testi başlatılmayacak."
                          : "Seçili RPM tahmini 50 MiB test sınırını aşıyor. Daha küçük bir grup seçin."
                    color: palette.text
                    font.pixelSize: metrics.fontSmall
                    wrapMode: Text.WordWrap
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
                                  && updates.selectedCount <= updateCache.maximumBatch)
                    text: updates.selectedCount > updateCache.maximumBatch
                          ? "Toplu indirme testinde en fazla " + updateCache.maximumBatch + " paket seçin."
                          : "İndirme için önce seçili paketlerin planını inceleyin ve sorgunun bitmesini bekleyin."
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSmall
                    color: palette.text
                    wrapMode: Text.WordWrap
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: updateCache.downloading || updateCache.downloadLog.length > 0
                    spacing: metrics.spaceCompact

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: updateCache.downloading ? "RPM indiriliyor" : "Son indirme"
                            font.bold: true
                            font.pixelSize: metrics.fontSmall
                            color: palette.text
                        }
                        Label {
                            text: updateCache.elapsedSeconds + " sn"
                            font.pixelSize: metrics.fontSmall
                            color: palette.text
                        }
                        ThemedActionButton {
                            text: downloadDetails.expanded ? "Günlüğü gizle" : "Günlüğü göster"
                            Layout.preferredHeight: Math.max(32, metrics.fontPx(32))
                            Layout.preferredWidth: Math.max(142, implicitWidth + 12)
                            font.pixelSize: metrics.fontSmall
                            onClicked: downloadDetails.expanded = !downloadDetails.expanded
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: updateCache.downloadFile.length > 0
                              ? updateCache.downloadFile : "DNF5 hazırlığı / depo denetimi"
                        font.pixelSize: metrics.fontSmall
                        color: palette.text
                        elide: Text.ElideMiddle
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: Math.max(0, updateCache.downloadPercent)
                        indeterminate: updateCache.downloading && updateCache.downloadPercent < 0
                    }
                    Label {
                        Layout.fillWidth: true
                        text: (updateCache.downloadPercent >= 0
                               ? "Son bildirilen RPM ilerlemesi: %" + updateCache.downloadPercent + " · "
                               : "Toplam ilerleme henüz belirlenemiyor · ")
                              + "Geçici dosyalar: " + page.formatCacheBytes(updateCache.stagedBytes)
                        font.pixelSize: metrics.fontSmall
                        color: palette.text
                        wrapMode: Text.WordWrap
                    }

                    ColumnLayout {
                        id: downloadDetails
                        property bool expanded: false
                        Layout.fillWidth: true
                        visible: expanded
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 160
                            TextArea {
                                text: updateCache.downloadLog
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.WrapAnywhere
                                font.family: "monospace"
                                font.pixelSize: metrics.fontSmall
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: updateCache.hasDownloadResult
                    text: "Son indirme: " + updateCache.lastNewCount + " yeni RPM · "
                          + updateCache.lastReusedCount + " önbellekten · "
                          + updateCache.lastRejectedCount + " doğrulanamayan dosya"
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSmall
                    font.bold: true
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

        Frame {
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: metrics.spaceCompact
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: "Gelişmiş: Fedora güncelleme durumu"
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                        font.bold: true
                        color: palette.text
                        wrapMode: Text.WordWrap
                    }
                    ThemedActionButton {
                        text: updateCache.offlineCheckRunning ? "Denetleniyor..." : "Durumu denetle"
                        enabled: !updateCache.offlineCheckRunning && !updateCache.downloading
                        Layout.preferredWidth: Math.max(150, implicitWidth + 14)
                        Layout.preferredHeight: Math.max(32, metrics.fontPx(32))
                        font.pixelSize: metrics.fontSmall
                        onClicked: updateCache.checkOfflineStatus()
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: "Sistem paketleri ve kernel güncellemelerinin güvenli uygulanması için çevrimdışı DNF5 işlemi ayrıca hazırlanmalıdır. Bu alan yalnızca mevcut durumu okur; yeniden başlatmaz veya paket kurmaz."
                    font.pixelSize: metrics.fontSmall
                    color: palette.text
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: updateCache.offlineStatus.length > 0
                    text: updateCache.offlineStatus
                    font.pixelSize: metrics.fontSmall
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
