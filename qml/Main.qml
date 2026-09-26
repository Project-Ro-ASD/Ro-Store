import QtQuick
import QtQuick.Controls
import RoStore 1.0
import "pages"

ApplicationWindow {
    id: root

    width: 1000
    height: 650
    visible: true
    title: "Ro-Store"
    color: "#101418"

    CatalogModel {
        id: catalogModel
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

    StackView {
        id: stackView
        anchors.fill: parent

        initialItem: HomePage {
            catalog: catalogModel

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

            onBackRequested: {
                stackView.pop()
            }

            onDownloadsRequested: {
                stackView.push(downloadsPageComponent)
            }
        }
    }

    Component.onCompleted: {
        catalogModel.load("https://repo.ro-asd.org/rpm/fedora/44/beta/store/catalog.json")
    }
}
