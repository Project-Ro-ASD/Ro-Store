import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import RoStore 1.0

Item {
    id: page
    signal backRequested()
    property var packageTransactionManager: null
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

    FlatpakManager {
        id: flatpak
        transactionManager: page.packageTransactionManager
        onChanged: page.updateRows()
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

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: Math.max(0, parent.width)
            spacing: 12
            anchors.margins: 12

            RowLayout {
                Layout.fillWidth: true
                ThemedActionButton {
                    text: "← Geri"
                    onClicked: page.backRequested()
                }
                Label {
                    text: "Flatpak / Flathub"
                    font.bold: true
                    font.pixelSize: 22
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: flatpak.message
                color: palette.windowText
            }
            Label {
                Layout.fillWidth: true
                visible: flatpak.outcomeUnknown
                wrapMode: Text.WordWrap
                font.bold: true
                color: "#d97706"
                text: "Güvenlik kilidi: Önceki Flatpak işleminin sonucu manuel doğrulanmalı. "
                      + "Yeni Flatpak paket değişiklikleri engellendi."
            }
            RowLayout {
                Layout.fillWidth: true
                ThemedActionButton {
                    text: "Yenile"
                    enabled: !flatpak.busy && !flatpak.loading
                    onClicked: flatpak.refresh()
                }
                ThemedActionButton {
                    text: "Durumu yeniden kontrol et"
                    visible: flatpak.outcomeUnknown
                    enabled: !flatpak.loading && !flatpak.busy
                    onClicked: flatpak.checkRecovery()
                }
                ThemedActionButton {
                    text: "Kontrol ettim, kilidi kaldır"
                    visible: flatpak.outcomeUnknown && flatpak.recoveryChecked
                    enabled: !flatpak.loading && !flatpak.busy
                    onClicked: recoveryConfirm.open()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Kurulum kapsamı:" }
                ComboBox {
                    id: scopeBox
                    Layout.fillWidth: true
                    model: ["Kullanıcı", "Sistem"]
                    onActivated: page.selectedScope = currentIndex === 0 ? "user" : "system"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ThemedActionButton {
                    Layout.fillWidth: true
                    text: "Uygulama keşfi"
                    onClicked: page.tab = "catalog"
                }
                ThemedActionButton {
                    Layout.fillWidth: true
                    text: "Kurulu"
                    onClicked: page.tab = "installed"
                }
                ThemedActionButton {
                    Layout.fillWidth: true
                    text: "Güncellemeler"
                    onClicked: page.tab = "updates"
                }
                ThemedActionButton {
                    Layout.fillWidth: true
                    text: "Kaynaklar"
                    onClicked: page.tab = "sources"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: page.tab === "catalog"
                Label { text: "Kaynak:" }
                ComboBox {
                    id: remoteBox
                    Layout.fillWidth: true
                    model: {
                        var matches = []
                        for (var i = 0; i < flatpak.sources.length; ++i) {
                            var r = flatpak.sources[i]
                            if (r.scope === page.selectedScope)
                                matches.push(r.name)
                        }
                        return matches
                    }
                    onActivated: page.selectedRemote = currentText
                }
                ThemedActionButton {
                    text: "Kataloğu yükle"
                    enabled: !page.blocked
                    onClicked: {
                        page.selectedRemote = remoteBox.currentText
                        flatpak.loadCatalog(page.selectedScope, page.selectedRemote)
                    }
                }
            }
            TextField {
                id: search
                Layout.fillWidth: true
                placeholderText: "Uygulama veya kimlik ara..."
                onTextChanged: page.updateRows()
            }
            ThemedActionButton {
                visible: page.tab === "sources" && page.selectedScope === "user"
                         && !flatpak.hasRemote("user", "flathub")
                text: "Flathub kullanıcı deposunu ekle"
                enabled: !page.blocked
                onClicked: page.request("flathub", {scope:"user", name:"Flathub", origin:"flathub"})
            }
            Label {
                Layout.fillWidth: true
                visible: !flatpak.available
                wrapMode: Text.WordWrap
                text: "Flatpak sistemde kurulu değil. Fedora paket yöneticisi üzerinden Flatpak'ı kurduktan sonra yenile."
            }
            Label {
                Layout.fillWidth: true
                visible: page.visibleRows.length === 0 && !flatpak.loading
                wrapMode: Text.WordWrap
                text: "Bu görünümde sonuç yok. Kaynakları yenile veya uygun depoyu seç."
            }
            Repeater {
                model: page.visibleRows
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: cardColumn.implicitHeight + 20
                    radius: 9
                    color: palette.base
                    border.color: palette.mid
                    ColumnLayout {
                        id: cardColumn
                        x: 10
                        y: 10
                        width: parent.width - 20
                        spacing: 5
                        Label {
                            Layout.fillWidth: true
                            text: modelData.name || modelData.id || modelData.url || ""
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: (modelData.id || modelData.url || "") + "  ·  "
                                  + (modelData.version || modelData.branch || "")
                            wrapMode: Text.WrapAnywhere
                            color: palette.windowText
                        }
                        Label {
                            Layout.fillWidth: true
                            text: page.scopeName(modelData.scope) + " · " + (modelData.origin || modelData.name || "")
                            color: palette.windowText
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: page.tab !== "sources"
                            ThemedActionButton {
                                visible: page.tab === "catalog" && !flatpak.isInstalled(modelData.scope, modelData.id)
                                text: "Kur"
                                enabled: !page.blocked
                                onClicked: page.request("install", modelData)
                            }
                            ThemedActionButton {
                                visible: page.tab === "installed"
                                text: "Çalıştır"
                                enabled: !flatpak.busy
                                onClicked: flatpak.launch(modelData.scope, modelData.id)
                            }
                            ThemedActionButton {
                                visible: page.tab === "installed"
                                text: "Kaldır"
                                enabled: !page.blocked
                                onClicked: page.request("uninstall", modelData)
                            }
                            ThemedActionButton {
                                visible: page.tab === "updates" || (page.tab === "installed"
                                         && flatpak.hasUpdate(modelData.scope, modelData.id))
                                text: "Güncelle"
                                enabled: !page.blocked
                                onClicked: page.request("update", modelData)
                            }
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: "Son işlem günlüğü"
                font.bold: true
                visible: flatpak.operationLog.length > 0
            }
            TextArea {
                Layout.fillWidth: true
                readOnly: true
                visible: flatpak.operationLog.length > 0
                text: flatpak.operationLog
                wrapMode: TextEdit.WrapAnywhere
                color: palette.text
            }
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
