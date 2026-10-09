import QtQuick
import QtQuick.Controls
import RoStore 1.0
import "pages"

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
    color: themePalette.window

    CatalogModel {
        id: catalogModel
    }

    SourceManager {
        id: sourceManager
    }

    PackageTransactionManager {
        id: transactionManager

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
            if (current && current.transactionRunning)
                return
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

            onBackRequested: {
                stackView.pop()
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
