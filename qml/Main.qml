import QtQuick
import QtQuick.Controls
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
    property bool exitingFromTray: false

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
            if (transactionManager.busy)
                exitConfirmation.open()
            else {
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
                text: "Paket kurulumu veya kaldırma işlemi devam ediyor. Uygulamadan tamamen çıkarsanız işlem kesilebilir. Emin misiniz?"
            }
            Row {
                spacing: 8
                ThemedActionButton {
                    text: "Arka planda sürdür"
                    onClicked: {
                        exitConfirmation.close()
                        if (TrayController.available) {
                            root.hide()
                            TrayController.notifyBackground()
                        }
                    }
                }
                ThemedActionButton {
                    text: "Vazgeç"
                    onClicked: exitConfirmation.close()
                }
                ThemedActionButton {
                    text: "Yine de çık"
                    onClicked: {
                        exitConfirmation.close()
                        root.exitingFromTray = true
                        Qt.quit()
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

            transactionManager.executeActive()
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

    StackView {
        id: stackView
        anchors.fill: parent

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
        id: fedoraCatalogComponent
        FedoraCatalogPage {
            packageTransactionManager: transactionManager
            onBackRequested: stackView.pop()
            onDownloadsRequested: stackView.push(downloadsPageComponent)
            onPackageSelected: function(name, summary, version, repository) {
                stackView.push(detailPageComponent, {
                    appName: name,
                    summaryText: summary,
                    descriptionText: summary,
                    categoryText: "Fedora RPM",
                    versionText: version,
                    packageName: name,
                    packageRepository: repository,
                    iconUrl: ""
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
