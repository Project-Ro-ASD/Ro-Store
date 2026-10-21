import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import RoStore 1.0

Item {
    id: page
    signal backRequested()
    signal appDetailsRequested(var appData)
    property var packageTransactionManager: null
    property var flatpak: null
    readonly property bool blocked: flatpak.busy || flatpak.loading || flatpak.outcomeUnknown
    property string selectedScope: "user"
    property string selectedRemote: "flathub"
    property string requestedAction: ""
    property string requestedId: ""
    property string requestedRemote: ""
    property string requestedScope: ""
    property string requestedName: ""
    property var visibleRows: []
    property string tab: "catalog"
    property int visibleLimit: 48
    onVisibleRowsChanged: visibleLimit = 48

    SystemPalette { id: palette }

    function scopeName(value) {
        return value === "user" ? "Kullanıcı" : "Sistem"
    }
    function updateRows() {
        var source = tab === "catalog" ? flatpak.catalog
                   : tab === "installed" ? flatpak.installed
                   : tab === "updates" ? flatpak.updates
                   : flatpak.sources
        var filter = search.text.toLocaleLowerCase()
        var output = []
        for (var i = 0; i < source.length; ++i) {
            var row = source[i]
            if (tab === "catalog" && (row.scope !== selectedScope || row.origin !== selectedRemote))
                continue
            if (tab !== "catalog" && tab !== "sources" && row.scope !== selectedScope)
                continue
            if (tab === "sources" && row.scope !== selectedScope)
                continue
            if (filter && ((row.name || "") + " " + (row.id || "")).toLocaleLowerCase().indexOf(filter) < 0)
                continue
            output.push(row)
        }
        page.visibleRows = output
    }
    function request(action, row) {
        if (page.blocked) return
        requestedAction = action
        requestedScope = row.scope || selectedScope
        requestedId = row.id || ""
        requestedRemote = row.origin || selectedRemote
        requestedName = row.name || requestedId || "Flathub"
        approval.open()
    }
    function executeRequest() {
        if (requestedAction === "install")
            flatpak.install(requestedScope, requestedRemote, requestedId)
        else if (requestedAction === "uninstall")
            flatpak.uninstall(requestedScope, requestedId)
        else if (requestedAction === "update")
            flatpak.update(requestedScope, requestedId)
        else if (requestedAction === "flathub")
            flatpak.addFlathubUser()
    }

    Connections {
        target: flatpak
        function onInventoryChanged() { page.updateRows() }
    }

    Component.onCompleted: flatpak.refresh()
    onSelectedScopeChanged: updateRows()
    onSelectedRemoteChanged: updateRows()
    onTabChanged: updateRows()

    Dialog {
        id: approval
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        title: "Flatpak işlemini onayla"
        width: Math.min(560, page.width - 28)
        standardButtons: Dialog.NoButton
        ColumnLayout {
            width: parent.width
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.bold: true
                text: (requestedAction === "uninstall" ? "Kaldırma" :
                       requestedAction === "update" ? "Güncelleme" :
                       requestedAction === "flathub" ? "Flathub kaynağı ekleme" : "Kurulum")
                      + " işlemini onaylıyor musun?"
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Paket: " + requestedName + "\nKapsam: "
                      + page.scopeName(requestedScope) + "\nKaynak: " + requestedRemote
                      + "\n\nFlatpak ek bağımlılıklar indirebilir ve paket dosyalarını değiştirebilir."
                      + (requestedAction === "uninstall"
                         ? "\nKaldırılan uygulamanın kullanıcı verileri otomatik silinmez." : "")
            }
            RowLayout {
                Layout.fillWidth: true
                ThemedActionButton {
                    Layout.fillWidth: true
                    text: "Vazgeç"
                    onClicked: approval.close()
                }
                ThemedActionButton {
                    Layout.fillWidth: true
                    accented: true
                    text: "Onayla ve uygula"
                    onClicked: {
                        approval.close()
                        page.executeRequest()
                    }
                }
            }
        }
    }


    ThemeMetrics { id: metrics }
    readonly property bool narrow: width < Math.max(760, Math.ceil(760 * metrics.fontScale))
    readonly property int margin: Math.max(12, Math.min(34, width / 18))
    property var selectedApp: null
    property int metadataRefresh: flatpak.metadataRevision

    function currentMetadata(id) {
        var unused = metadataRefresh
        return flatpak.metadata(id)
    }

    ScrollView {
        id: pageScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
        width: Math.max(0, pageScroll.availableWidth - page.margin * 2)
        x: page.margin
        y: page.margin
        spacing: metrics.spaceMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceMedium
            ThemedIconButton {
                text: "Geri"
                themedIconName: "go-previous"
                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                onClicked: page.backRequested()
            }
            Label {
                Layout.fillWidth: true
                text: "Flatpak / Flathub"
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontPageTitle
                font.bold: true
                color: palette.windowText
                elide: Text.ElideRight
            }
            ThemedIconButton {
                text: "Yenile"
                themedIconName: "view-refresh"
                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                enabled: !flatpak.loading && !flatpak.busy
                onClicked: flatpak.refresh()
            }
        }

        Label {
            Layout.fillWidth: true
            text: flatpak.message
            wrapMode: Text.WordWrap
            font.pixelSize: metrics.fontSmall
            color: palette.windowText
        }

        Rectangle {
            Layout.fillWidth: true
            visible: flatpak.outcomeUnknown
            implicitHeight: recoveryColumn.implicitHeight + 20
            radius: metrics.radiusPanel
            color: palette.base
            border.color: palette.mid
            ColumnLayout {
                id: recoveryColumn
                x: 10
                y: 10
                width: parent.width - 20
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    font.bold: true
                    wrapMode: Text.WordWrap
                    text: "Güvenlik kilidi: Flatpak işlemi manuel doğrulama bekliyor."
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    ThemedActionButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                        text: "Durumu kontrol et"
                        enabled: !flatpak.busy && !flatpak.loading
                        onClicked: flatpak.checkRecovery()
                    }
                    ThemedActionButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                        text: "Kaydı onayla"
                        enabled: flatpak.recoveryChecked && !flatpak.busy && !flatpak.loading
                        onClicked: recoveryConfirm.open()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceMedium
            Label {
                text: "Kapsam"
                font.pixelSize: metrics.fontSmall
            }
            ComboBox {
                id: scopeBox
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                model: ["Kullanıcı", "Sistem"]
                onActivated: page.selectedScope = currentIndex === 0 ? "user" : "system"
            }
            Label {
                text: "Kaynak"
                visible: !page.narrow && page.tab === "catalog"
                font.pixelSize: metrics.fontSmall
            }
            ComboBox {
                id: remoteBox
                visible: !page.narrow && page.tab === "catalog"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                model: {
                    var items = []
                    for (var i = 0; i < flatpak.sources.length; ++i)
                        if (flatpak.sources[i].scope === page.selectedScope)
                            items.push(flatpak.sources[i].name)
                    return items
                }
                onActivated: page.selectedRemote = currentText
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceCompact
            Repeater {
                model: [
                    {key:"catalog", label:"Keşfet"},
                    {key:"installed", label:"Kurulu"},
                    {key:"updates", label:"Güncellemeler"},
                    {key:"sources", label:"Kaynaklar"}
                ]
                delegate: ThemedActionButton {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                    text: modelData.label
                    accented: page.tab === modelData.key
                    onClicked: page.tab = modelData.key
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: page.tab === "catalog"
            spacing: 8
            ComboBox {
                visible: page.narrow
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                model: remoteBox.model
                onActivated: page.selectedRemote = currentText
            }
            ThemedActionButton {
                Layout.fillWidth: page.narrow
                Layout.minimumWidth: Math.max(152, metrics.fontPx(146))
                Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                text: "Kataloğu yükle"
                enabled: !page.blocked
                onClicked: flatpak.loadCatalog(page.selectedScope, page.selectedRemote)
            }
        }

        TextField {
            id: search
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
            placeholderText: "Uygulama adı veya kimliği ara..."
            onTextChanged: page.updateRows()
        }
        ThemedActionButton {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
            visible: page.tab === "sources" && page.selectedScope === "user"
                     && !flatpak.hasRemote("user", "flathub")
            enabled: !page.blocked
            text: "Flathub kullanıcı deposunu ekle"
            onClicked: page.request("flathub", {scope:"user", name:"Flathub", origin:"flathub"})
        }

        Label {
            Layout.fillWidth: true
            visible: !flatpak.available
            wrapMode: Text.WordWrap
            text: "Flatpak bulunamadı. Fedora paket yöneticisinden Flatpak kurduktan sonra yenileyin."
        }

        Label {
            Layout.fillWidth: true
            visible: !flatpak.loading && page.visibleRows.length === 0
            text: "Sonuç yok. Kaynağı değiştirin veya listeyi yenileyin."
            wrapMode: Text.WordWrap
            color: palette.windowText
        }

        Grid {
            id: appGrid
            Layout.fillWidth: true
            visible: page.visibleRows.length > 0
            columns: Math.max(1, Math.floor(width / Math.max(300, metrics.fontPx(330))))
            columnSpacing: 0
            rowSpacing: 0
            Repeater {
            model: page.visibleRows.slice(0, page.visibleLimit)
            delegate: Item {
                id: cell
                required property var modelData
                width: Math.max(1, Math.floor(appGrid.width / appGrid.columns))
                height: Math.max(225, metrics.fontPx(225))
                readonly property var info: page.currentMetadata(modelData.id || "")
                Component.onCompleted: {
                    if (page.tab !== "sources" && modelData.origin === "flathub")
                        flatpak.requestMetadata(modelData.id)
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 6
                    radius: metrics.radiusCard
                    color: palette.base
                    border.color: palette.mid
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Rectangle {
                                Layout.preferredWidth: 60
                                Layout.preferredHeight: 60
                                radius: metrics.radiusInner
                                color: metrics.iconTileColor(palette.base, palette.text)
                                border.color: metrics.iconTileBorderColor(palette.mid, palette.text)
                                Image {
                                    id: appIcon
                                    anchors.centerIn: parent
                                    width: 46
                                    height: 46
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    source: cell.info.iconUrl || ""
                                    visible: status === Image.Ready
                                }
                                Label {
                                    anchors.centerIn: parent
                                    visible: appIcon.status !== Image.Ready
                                    text: (cell.modelData.name || cell.modelData.id || "?").slice(0,1).toUpperCase()
                                    font.bold: true
                                    font.pixelSize: metrics.fontPx(26)
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: cell.modelData.name || cell.modelData.id || cell.modelData.url || ""
                                    font.pixelSize: metrics.fontTitle
                                    font.bold: true
                                    color: palette.text
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: cell.modelData.id || cell.modelData.url || ""
                                    font.pixelSize: metrics.fontCaption
                                    color: palette.windowText
                                    elide: Text.ElideMiddle
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: page.scopeName(cell.modelData.scope)
                                          + " · " + (cell.modelData.origin || cell.modelData.name || "—")
                                    font.pixelSize: metrics.fontCaption
                                    color: palette.windowText
                                    elide: Text.ElideRight
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: page.tab === "sources"
                                  ? (cell.modelData.url || "Depo bilgisi")
                                  : (cell.info.summary || cell.info.description || "Açıklama bilgisi mevcut değil.")
                            wrapMode: Text.WordWrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                            font.pixelSize: metrics.fontBody
                            color: palette.text
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: page.tab !== "sources"
                            text: cell.modelData.version
                                  ? "Sürüm: " + cell.modelData.version
                                  : "Sürüm bilgisi mevcut değil"
                            font.pixelSize: metrics.fontCaption
                            color: palette.windowText
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 7
                            ThemedActionButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                                visible: page.tab === "catalog"
                                text: flatpak.isInstalled(cell.modelData.scope, cell.modelData.id)
                                        ? "Kurulu" : "Kur"
                                enabled: !page.blocked && !flatpak.isInstalled(cell.modelData.scope, cell.modelData.id)
                                accented: enabled
                                onClicked: page.request("install", cell.modelData)
                            }
                            ThemedActionButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                                visible: page.tab === "installed"
                                text: "Çalıştır"
                                enabled: !flatpak.busy
                                onClicked: flatpak.launch(cell.modelData.scope, cell.modelData.id)
                            }
                            ThemedActionButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                                visible: page.tab === "installed"
                                text: "Kaldır"
                                enabled: !page.blocked
                                onClicked: page.request("uninstall", cell.modelData)
                            }
                            ThemedActionButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                                visible: page.tab === "updates"
                                         || (page.tab === "installed" && flatpak.hasUpdate(cell.modelData.scope, cell.modelData.id))
                                text: flatpak.isInstalled(cell.modelData.scope, cell.modelData.id)
                                      ? "Güncelle" : "Uzantı (salt okunur)"
                                enabled: !page.blocked
                                         && flatpak.isInstalled(cell.modelData.scope, cell.modelData.id)
                                         && flatpak.hasUpdate(cell.modelData.scope, cell.modelData.id)
                                onClicked: page.request("update", cell.modelData)
                            }
                            ThemedActionButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                                visible: page.tab !== "sources"
                                text: "Detay"
                                onClicked: {
                                    page.appDetailsRequested(cell.modelData)
                                }
                            }
                        }
                    }
                }
            }
            }
        }

        ThemedActionButton {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(44, metrics.fontPx(44))
            visible: page.visibleRows.length > page.visibleLimit
            text: "Daha fazla göster (" + Math.min(page.visibleLimit, page.visibleRows.length)
                  + " / " + page.visibleRows.length + ")"
            onClicked: page.visibleLimit += 48
        }
        TextArea {
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            visible: flatpak.operationLog.length > 0
            readOnly: true
            wrapMode: TextEdit.WrapAnywhere
            text: flatpak.operationLog
            color: palette.text
        }
        Item { Layout.fillWidth: true; Layout.preferredHeight: page.margin }
        }
    }

    Dialog {
        id: recoveryConfirm
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: "Güvenlik kaydını kaldır"
        width: Math.min(500, page.width - 28)
        standardButtons: Dialog.NoButton
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Önceki Flatpak işlem sonucunu kurulu uygulamalar ve kaynaklardan "
                      + "doğruladın mı? Bu onay işlemin başarılı olduğunu kanıtlamaz. "
                      + "Şüpheli durumda kilidi kaldırma."
            }
            RowLayout {
                Layout.fillWidth: true
                ThemedActionButton {
                    Layout.fillWidth: true
                    text: "Vazgeç"
                    onClicked: recoveryConfirm.close()
                }
                ThemedActionButton {
                    Layout.fillWidth: true
                    text: "Doğruladım"
                    onClicked: {
                        recoveryConfirm.close()
                        flatpak.acknowledgeRecovery()
                    }
                }
            }
        }
    }
}
