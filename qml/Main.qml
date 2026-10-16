import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import RoStore 1.0
import "pages"
import "components"

ApplicationWindow {
    id: root

    // Qt Quick Controls (Button, TextField, ComboBox, etc.) inherit this font.
    // Without an explicit notifying binding, existing controls can retain
    // the previous KDE font after a live 18 pt -> 10 pt change.
    font: SystemFontMonitor.currentFont

    SystemPalette {
        id: themePalette
    }
    width: 1000
    height: 650
    visible: true
    title: "Ro-Store"
    property bool qaSafetyBannerPreview: false
    property bool exitingFromTray: false
    property var pendingConfirmation: null

    function formatSize(bytes) {
        var value = Number(bytes)
        if (!isFinite(value) || value <= 0) return "0 MB"
        return value < 1073741824
             ? (value / 1048576).toFixed(2) + " MB"
             : (value / 1073741824).toFixed(2) + " GB"
    }

    function resolvedPlanText(transaction) {
        if (!transaction) return ""
        var items = transaction.resolvedItems
        var lines = []
        for (var i = 0; i < items.length; ++i) {
            var row = items[i]
            var pkg = row.object || {}
            lines.push((i + 1) + ". " + (row.action || "İşlem") + " — "
                       + (pkg.name || "?") + " " + (pkg.version || "")
                       + " [" + (row.reason || "—") + "]")
        }
        return lines.join("\n")
    }


    // The system tray keeps the QML engine and transaction manager alive.
    // If KDE provides no tray, do not allow closing during a package operation.
    onClosing: function(close) {
        if (root.exitingFromTray)
            return
        close.accepted = false
        if (transactionManager.busy) {
            exitConfirmation.open()
        } else if (TrayController.available) {
            root.hide()
            TrayController.notifyBackground()
        } else {
            root.exitingFromTray = true
            Qt.quit()
        }
    }

    Connections {
        target: TrayController
        function onShowRequested() {
            root.show()
            root.raise()
            root.requestActivate()
        }
        function onQuitRequested() {
            if (transactionManager.busy) {
                root.show()
                root.raise()
                root.requestActivate()
                exitConfirmation.open()
            } else {
                root.exitingFromTray = true
                Qt.quit()
            }
        }
    }
    color: themePalette.window

    Dialog {
        id: exitConfirmation
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: "Devam eden işleminiz var"
        standardButtons: Dialog.NoButton
        width: Math.min(440, root.width - 32)
        Column {
            width: parent.width
            spacing: 16
            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                text: "Paket işlemi veya sistem güncelleme planı / çevrimdışı hazırlığı sürüyor. Uygulamadan tamamen çıkmak DNF5 işlem sonucunu belirsiz bırakabilir. Devam etmek istediğinize emin misiniz?"
            }
            Column {
                width: parent.width
                spacing: 8
                ThemedActionButton {
                    width: parent.width
                    height: Math.max(38, implicitHeight)
                    visible: TrayController.available
                    text: "Arka planda sürdür"
                    onClicked: {
                        exitConfirmation.close()
                        root.hide()
                        TrayController.notifyBackground()
                    }
                }
                ThemedActionButton {
                    width: parent.width
                    height: Math.max(38, implicitHeight)
                    text: "Vazgeç"
                    onClicked: exitConfirmation.close()
                }
                ThemedActionButton {
                    width: parent.width
                    height: Math.max(38, implicitHeight)
                    text: "Yine de çık (işlem kesilebilir)"
                    onClicked: {
                        exitConfirmation.close()
                        root.exitingFromTray = true
                        Qt.quit()
                    }
                }
            }
        }
    }

    Dialog {
        id: transactionApproval
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        title: "Paket işlemini onayla"
        width: Math.min(root.width - 24, 610)
        height: Math.min(root.height - 24, 550)
        standardButtons: Dialog.NoButton

        onClosed: {
            if (root.pendingConfirmation) {
                transactionManager.cancelTransaction(root.pendingConfirmation)
                root.pendingConfirmation = null
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.bold: true
                text: root.pendingConfirmation
                      ? root.pendingConfirmation.packageName + " — "
                        + root.pendingConfirmation.resolvedItemCount + " paket işlemi"
                      : ""
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.pendingConfirmation
                      ? (root.pendingConfirmation.totalBytes > 0
                          ? "Tahmini indirme: " + root.formatSize(root.pendingConfirmation.totalBytes)
                            + " (DNF5 indirme başlangıcında kesinleşebilir). "
                          : "Tahmini indirme boyutu henüz bilinmiyor; DNF5 paket indirmeye "
                            + "başladığında gerçek boyut gösterilecek. ")
                        + "Ek bağımlılıkları ve kaldırılacak paketleri inceleyin."
                      : ""
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                TextEdit {
                    width: parent.width
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: themePalette.text
                    text: root.resolvedPlanText(root.pendingConfirmation)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ThemedActionButton {
                    text: "Vazgeç"
                    Layout.fillWidth: true
                    Layout.minimumHeight: 42
                    onClicked: transactionApproval.close()
                }
                ThemedActionButton {
                    text: "Onayla ve uygula"
                    accented: true
                    Layout.fillWidth: true
                    Layout.minimumHeight: 42
                    onClicked: {
                        if (!root.pendingConfirmation)
                            return
                        root.pendingConfirmation = null
                        transactionApproval.close()
                        transactionManager.executeActive()
                    }
                }
            }
        }
    }

    CatalogModel {
        id: catalogModel
    }

    SourceManager {
        id: sourceManager
    }

    PackageTransactionManager {
        id: transactionManager
        onBusyChanged: TrayController.setBusy(busy)

        onTransactionActivated: function(transaction) {
            console.log(
                "MANAGER ACTIVE:",
                transaction.packageName
            )
        }

        onTransactionResolved: function(transaction) {
            console.log(
                "MANAGER READY:",
                transaction.packageName,
                "state:",
                transaction.state,
                "items:",
                transaction.resolvedItemCount,
                "download:",
                transaction.totalBytes
            )

            if (!transactionManager.outcomeUncertain
                    && transaction.state === PackageTransaction.Ready) {
                root.pendingConfirmation = transaction
                transactionApproval.open()
            }
        }
    }

    // One window-level Escape handler: dismiss modal first, then Back.
    Shortcut {
        sequence: "Escape"
        context: Qt.WindowShortcut
        enabled: stackView.depth > 1

        onActivated: {
            var current = stackView.currentItem
            if (current && current.dismissOnEscape
                    && current.dismissOnEscape())
                return
            // The manager lives for the whole app lifetime.
            // Escape navigates while operations continue in Downloads.
            stackView.pop()
        }
    }

    Dialog {
        id: recoveryInfoDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: "DNF5 güvenlik kontrolü"
        standardButtons: Dialog.Close
        width: Math.min(root.width - 24, 610)
        height: Math.min(root.height - 24, 450)
        ColumnLayout {
            anchors.fill: parent
            Label {
                text: "Önceki paket işleminin sonucu kesinleşmedi. Yeni işlemler engellendi."
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextEdit {
                    width: parent.width
                    text: transactionManager.recoveryDetails
                    color: themePalette.text
                    wrapMode: TextEdit.Wrap
                    readOnly: true
                    selectByMouse: true
                }
            }
            ThemedActionButton {
                text: "RPM ve DNF geçmişini tekrar kontrol et"
                Layout.fillWidth: true
                onClicked: transactionManager.recheckRecovery()
            }
            ThemedActionButton {
                visible: transactionManager.recoveryVerified
                text: "Doğrulamayı onayla ve kilidi kaldır"
                Layout.fillWidth: true
                onClicked: {
                    transactionManager.acknowledgeRecovery()
                    if (!transactionManager.outcomeUncertain)
                        recoveryInfoDialog.close()
                }
            }
        }
    }

    Rectangle {
        id: transactionSafetyNotice
        visible: transactionManager.outcomeUncertain || root.qaSafetyBannerPreview
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(66, noticeLabel.implicitHeight + 20)
        z: 100
        color: themePalette.base
        border.color: themePalette.highlight
        border.width: 2
        Label {
            id: noticeLabel
            anchors.fill: parent
            anchors.margins: 10
            text: root.qaSafetyBannerPreview && !transactionManager.outcomeUncertain
                  ? "ARAYÜZ TESTİ: DNF5 güvenlik uyarısı görünüm önizlemesi. Paket işlemi yapılmıyor."
                  : "DNF5 işlem sonucu belirsiz! Yeni paket işlemleri durduruldu. " +
                    "Önce Yüklemeler ekranındaki teknik günlüğü ve sistem paket durumunu kontrol edin."
            color: themePalette.text
            wrapMode: Text.WordWrap
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (transactionManager.outcomeUncertain)
                    recoveryInfoDialog.open()
            }
        }
    }

    StackView {
        id: stackView
        anchors.top: transactionSafetyNotice.visible
                     ? transactionSafetyNotice.bottom : parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        initialItem: HomePage {
            catalog: catalogModel
            packageTransactionManager: transactionManager
            sourceManager: sourceManager

            onRepositoryActionRequested: {
                sourceManager.executeRoAsdAction()
            }

            onReloadRequested: {
                catalogModel.load("https://repo.ro-asd.org/rpm/fedora/44/beta/store/catalog.json")
            }

            onDownloadsRequested: {
                stackView.push(downloadsPageComponent)
            }

            onFedoraCatalogRequested: {
                stackView.push(fedoraCatalogComponent)
            }
            onSystemUpdatesRequested: {
                stackView.push(systemUpdatesPageComponent)
            }

            onAppSelected: function(title, summary, description, category, version, packageName, iconUrl) {
                stackView.push(detailPageComponent, {
                    appName: title,
                    summaryText: summary,
                    descriptionText: description,
                    categoryText: category,
                    versionText: version,
                    packageName: packageName,
                    iconUrl: iconUrl
                })
            }
        }
    }

    Component {
        id: downloadsPageComponent

        DownloadsPage {
            packageTransactionManager: transactionManager
            onPackageRequested: function(tx) {
                var fedoraRepo = tx.repositoryId && tx.repositoryId !== "ro-asd-beta"
                stackView.push(detailPageComponent, {
                    appName: tx.packageName,
                    packageName: tx.packageName,
                    initialTransaction: tx,
                    packageRepository: fedoraRepo ? tx.repositoryId : "ro-asd",
                    categoryText: fedoraRepo ? "Fedora RPM" : "Ro-ASD",
                    summaryText: "",
                    descriptionText: "",
                    versionText: "",
                    iconUrl: ""
                })
            }

            onBackRequested: {
                stackView.pop()
            }
        }
    }

    Component {
        id: systemUpdatesPageComponent
        SystemUpdatesPage {
            packageTransactionManager: transactionManager
            onBackRequested: stackView.pop()
        }
    }

    Component {
        id: fedoraCatalogComponent
        FedoraCatalogPage {
            packageTransactionManager: transactionManager
            onBackRequested: stackView.pop()
            onDownloadsRequested: stackView.push(downloadsPageComponent)
            onPackageSelected: function(name, summary, version, repository, displayName, category, iconUrl, description, screenshots) {
                stackView.push(detailPageComponent, {
                    appName: displayName || name,
                    summaryText: summary,
                    descriptionText: description || summary,
                    screenshotUrls: screenshots || [],
                    categoryText: category || "Fedora RPM",
                    versionText: version,
                    packageName: name,
                    packageRepository: repository,
                    iconUrl: iconUrl || ""
                })
            }
        }
    }

    Component {
        id: detailPageComponent

        AppDetailPage {
            packageTransactionManager: transactionManager
            repositorySourceManager: sourceManager

            onBackRequested: {
                stackView.pop()
            }

            onDownloadsRequested: {
                stackView.push(downloadsPageComponent)
            }
        }
    }

    Component.onCompleted: {
        sourceManager.refresh()

        catalogModel.load(
            "https://repo.ro-asd.org/rpm/fedora/44/beta/store/catalog.json"
        )
    }
}
