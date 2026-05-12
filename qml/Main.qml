pragma ComponentBehavior: Bound

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
    readonly property url catalogUrl: "https://project-ro-asd.github.io/Ro-Repo/store/catalog.json"

    CatalogModel {
        id: catalogModel
    }

    StackView {
        id: stackView
        anchors.fill: parent

        initialItem: HomePage {
            catalog: catalogModel

            onReloadRequested: {
                catalogModel.load(root.catalogUrl)
            }

            onAppSelected: function(title, summary, description, category, version, packageName, installPackage, launchCommand, iconUrl) {
                stackView.push(detailPageComponent, {
                    appName: title,
                    summaryText: summary,
                    descriptionText: description,
                    categoryText: category,
                    versionText: version,
                    packageName: packageName,
                    installPackage: installPackage,
                    launchCommand: launchCommand,
                    iconUrl: iconUrl
                })
            }
        }
    }

    Component {
        id: detailPageComponent

        AppDetailPage {
            onBackRequested: {
                stackView.pop()
            }
        }
    }

    Component.onCompleted: {
        catalogModel.load(root.catalogUrl)
    }
}
