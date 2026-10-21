import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: page
    signal backRequested()

    // Reuse the window-owned backend. The detail view never creates a second manager.
    property var flatpak: null
    property var appData: ({})
    property string pendingAction: ""

    // Q_INVOKABLE metadata() itself has no QML change notification. Explicitly
    // depend on the revision so late screenshot responses update this page.
    readonly property int metadataRevision: flatpak ? flatpak.metadataRevision : 0
    readonly property var info: {
        var revision = metadataRevision
        return flatpak ? flatpak.metadata(appData.id || "") : ({})
    }
    readonly property bool installed: flatpak && flatpak.isInstalled(appData.scope || "", appData.id || "")
    readonly property bool updatable: flatpak && flatpak.hasUpdate(appData.scope || "", appData.id || "")
    readonly property bool blocked: !flatpak || flatpak.busy || flatpak.loading || flatpak.outcomeUnknown
    readonly property bool narrow: width < Math.max(660, metrics.fontPx(660))
    readonly property int margin: Math.max(12, Math.min(36, Math.floor(width / 17)))

    ThemeMetrics { id: metrics }
    SystemPalette { id: palette }

    FlatpakScreenshotViewer {
        id: screenshotViewer
        screenshotUrls: page.info.screenshots || []
        applicationName: page.appData.name || page.appData.id || "Flatpak"
    }

    Component.onCompleted: {
        if (flatpak && appData.id && appData.origin === "flathub")
            flatpak.requestMetadata(appData.id)
    }

    function actionDescription(action) {
        switch (action) {
        case "install": return "Kur"
        case "uninstall": return "Kaldır"
        case "update": return "Güncelle"
        default: return ""
        }
    }

    function confirmAction(action) {
        if (blocked || !appData.id) return
        pendingAction = action
        actionApproval.open()
    }

    function executeApproved() {
        if (blocked || !appData.id) return
        if (pendingAction === "install")
            flatpak.install(appData.scope, appData.origin, appData.id)
        else if (pendingAction === "uninstall")
            flatpak.uninstall(appData.scope, appData.id)
        else if (pendingAction === "update")
            flatpak.update(appData.scope, appData.id)
    }

    Dialog {
        id: actionApproval
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        title: "Flatpak işlemini onayla"
        width: Math.min(560, Math.max(300, page.width - 28))
        standardButtons: Dialog.NoButton
        ColumnLayout {
            width: parent.width
            spacing: metrics.spaceNormal
            Label {
                Layout.fillWidth: true
                font.bold: true
                wrapMode: Text.WordWrap
                text: (page.appData.name || page.appData.id || "Uygulama") + " — "
                      + page.actionDescription(page.pendingAction) + " işlemi"
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Kapsam: " + (page.appData.scope === "user" ? "Kullanıcı" : "Sistem")
                      + "\nKaynak: " + (page.appData.origin || "Bilinmiyor")
                      + "\nKimlik: " + (page.appData.id || "")
                      + "\n\nFlatpak ek çalışma ortamları indirebilir veya mevcut paketleri değiştirebilir."
                      + (page.pendingAction === "uninstall"
                         ? "\nUygulamanın kullanıcı verileri silinmeyecektir." : "")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: metrics.spaceNormal
                ThemedActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                    text: "Vazgeç"
                    onClicked: actionApproval.close()
                }
                ThemedActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                    text: "Onayla ve uygula"
                    accented: true
                    enabled: !page.blocked
                    onClicked: {
                        actionApproval.close()
                        page.executeApproved()
                    }
                }
            }
        }
    }

    ScrollView {
        id: detailScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: Math.max(0, detailScroll.availableWidth - page.margin * 2)
            x: page.margin
            y: page.margin
            spacing: metrics.spaceMedium

            RowLayout {
                Layout.fillWidth: true
                spacing: metrics.spaceMedium
                ThemedIconButton {
                    text: "Flatpak listesine dön"
                    themedIconName: "go-previous"
                    Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                    onClicked: page.backRequested()
                }
                Label {
                    Layout.fillWidth: true
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontPageTitle
                    font.bold: true
                    color: palette.windowText
                    text: "Uygulama detayları"
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: appHeader.implicitHeight + metrics.spaceLarge * 2
                radius: metrics.radiusCard
                color: palette.base
                border.color: palette.mid

                RowLayout {
                    id: appHeader
                    x: metrics.spaceLarge
                    y: metrics.spaceLarge
                    width: Math.max(0, parent.width - metrics.spaceLarge * 2)
                    spacing: metrics.spaceMedium

                    Rectangle {
                        Layout.preferredWidth: Math.max(76, metrics.fontPx(76))
                        Layout.preferredHeight: Math.max(76, metrics.fontPx(76))
                        radius: metrics.radiusInner
                        color: metrics.iconTileColor(palette.base, palette.text)
                        border.color: metrics.iconTileBorderColor(palette.mid, palette.text)

                        Image {
                            id: appIcon
                            anchors.centerIn: parent
                            width: parent.width - 16
                            height: parent.height - 16
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            source: page.info.iconUrl || ""
                            visible: status === Image.Ready
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: appIcon.status !== Image.Ready
                            text: (page.appData.name || page.appData.id || "?").slice(0, 1).toUpperCase()
                            font.pixelSize: metrics.fontPx(34)
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: metrics.spaceCompact
                        Label {
                            Layout.fillWidth: true
                            text: page.appData.name || page.appData.id || "Uygulama"
                            font.family: metrics.systemFont.family
                            font.pixelSize: metrics.fontPx(24)
                            font.bold: true
                            color: palette.text
                            wrapMode: Text.WrapAnywhere
                        }
                        Label {
                            Layout.fillWidth: true
                            text: page.appData.id || ""
                            font.pixelSize: metrics.fontSmall
                            color: palette.windowText
                            wrapMode: Text.WrapAnywhere
                        }
                        Label {
                            Layout.fillWidth: true
                            text: page.info.summary || "Flatpak uygulaması"
                            font.pixelSize: metrics.fontBody
                            color: palette.text
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            FlatpakOperationPanel {
                Layout.fillWidth: true
                manager: flatpak
            }

            Rectangle {
                Layout.fillWidth: true
                visible: flatpak && flatpak.outcomeUnknown
                implicitHeight: lockMessage.implicitHeight + 22
                color: palette.base
                radius: metrics.radiusPanel
                border.color: palette.mid
                Label {
                    id: lockMessage
                    x: 11; y: 11
                    width: parent.width - 22
                    font.bold: true
                    text: "Güvenlik kilidi etkin. Önce Flatpak ekranındaki kurtarma denetimini tamamlayın."
                    wrapMode: Text.WordWrap
                }
            }

            Flow {
                Layout.fillWidth: true
                width: parent.width
                spacing: metrics.spaceNormal

                ThemedActionButton {
                    width: Math.max(130, metrics.fontPx(130))
                    height: Math.max(44, metrics.fontPx(44))
                    text: page.installed ? "Kurulu" : "Kur"
                    accented: !page.installed
                    visible: !page.installed
                    enabled: !page.blocked
                    onClicked: page.confirmAction("install")
                }
                ThemedActionButton {
                    width: Math.max(130, metrics.fontPx(130))
                    height: Math.max(44, metrics.fontPx(44))
                    text: "Çalıştır"
                    visible: page.installed
                    enabled: flatpak && !flatpak.busy
                    onClicked: flatpak.launch(page.appData.scope, page.appData.id)
                }
                ThemedActionButton {
                    width: Math.max(130, metrics.fontPx(130))
                    height: Math.max(44, metrics.fontPx(44))
                    text: "Güncelle"
                    visible: page.updatable && page.installed
                    enabled: !page.blocked
                    onClicked: page.confirmAction("update")
                }
                ThemedActionButton {
                    width: Math.max(130, metrics.fontPx(130))
                    height: Math.max(44, metrics.fontPx(44))
                    text: "Kaldır"
                    visible: page.installed
                    enabled: !page.blocked
                    onClicked: page.confirmAction("uninstall")
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Paket bilgileri"
                font.pixelSize: metrics.fontSection
                font.bold: true
                color: palette.windowText
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: packageInfo.implicitHeight + 24
                radius: metrics.radiusPanel
                color: palette.base
                border.color: palette.mid
                GridLayout {
                    id: packageInfo
                    x: 12; y: 12
                    width: Math.max(0, parent.width - 24)
                    columns: page.narrow ? 1 : 2
                    rowSpacing: metrics.spaceNormal
                    columnSpacing: metrics.spaceLarge

                    Label { Layout.fillWidth: true; text: "Kaynak: " + (page.appData.origin || "Bilinmiyor"); wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; text: "Kapsam: " + (page.appData.scope === "user" ? "Kullanıcı" : "Sistem"); wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; text: "Sürüm: " + (page.appData.version || "Belirtilmemiş"); wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; text: "Dal: " + (page.appData.branch || "Belirtilmemiş"); wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; text: "Durum: " + (page.installed ? "Kurulu" : "Kurulu değil"); wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; text: "Güncelleme: " + (page.updatable ? "Mevcut" : "Yok veya bilinmiyor"); wrapMode: Text.WordWrap }
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Hakkında"
                font.pixelSize: metrics.fontSection
                font.bold: true
                color: palette.windowText
            }
            Label {
                Layout.fillWidth: true
                text: page.info.description || page.info.summary
                      || "Bu uygulama için ayrıntılı açıklama henüz sağlanmadı."
                wrapMode: Text.WordWrap
                font.pixelSize: metrics.fontBody
                color: palette.windowText
            }

            Label {
                Layout.fillWidth: true
                text: "Ekran görüntüleri"
                font.pixelSize: metrics.fontSection
                font.bold: true
                color: palette.windowText
            }
            Label {
                Layout.fillWidth: true
                visible: !page.info.screenshots || page.info.screenshots.length === 0
                text: page.info.metadataStatus === "loading"
                    ? "Ekran görüntüleri yükleniyor..."
                    : page.info.metadataStatus === "error"
                      ? "Flathub görsel bilgileri alınamadı. Daha sonra tekrar deneyin."
                      : "Bu uygulamanın Flathub metadata kaydında gösterilebilir ekran görüntüsü bulunamadı."
                color: palette.windowText
                wrapMode: Text.WordWrap
            }

            ThemedActionButton {
                Layout.preferredWidth: Math.max(160, metrics.fontPx(190))
                Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                visible: appData.origin === "flathub"
                text: "Flathub sayfasını aç"
                onClicked: {
                    if (appData.id && /^[A-Za-z0-9][A-Za-z0-9_.-]{2,255}$/.test(appData.id))
                        Qt.openUrlExternally("https://flathub.org/apps/" + encodeURIComponent(appData.id))
                }
            }

            Flickable {
                id: gallery
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(208, metrics.fontPx(230))
                visible: !!page.info.screenshots && page.info.screenshots.length > 0
                contentWidth: galleryRow.width
                contentHeight: height
                clip: true
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds

                Row {
                    id: galleryRow
                    height: gallery.height - 14
                    spacing: metrics.spaceNormal
                    Repeater {
                        model: page.info.screenshots || []
                        delegate: Rectangle {
                            required property string modelData
                            required property int index
                            width: Math.max(240, Math.min(gallery.width - 10, 405))
                            height: galleryRow.height
                            radius: metrics.radiusPanel
                            color: palette.base
                            border.color: palette.mid
                            clip: true
                            Image {
                                anchors.fill: parent
                                anchors.margins: 6
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                                source: modelData
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: screenshotViewer.openAt(index)
                            }
                        }
                    }
                }
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
            }

            Label {
                Layout.fillWidth: true
                visible: flatpak && flatpak.operationLog.length > 0
                text: "Son işlem günlüğü"
                font.pixelSize: metrics.fontSection
                font.bold: true
            }

            Item { Layout.fillWidth: true; Layout.preferredHeight: page.margin }
        }
    }
}
