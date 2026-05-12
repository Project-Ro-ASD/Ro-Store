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

    StackView {
        id: stackView
        anchors.fill: parent

        initialItem: HomePage {
            catalog: catalogModel

            onReloadRequested: {
                catalogModel.load("https://project-ro-asd.github.io/Ro-Repo/store/catalog.json")
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
        id: detailPageComponent

        AppDetailPage {
            onBackRequested: {
                stackView.pop()
            }
        }
    }

    Component.onCompleted: {
        catalogModel.load("https://project-ro-asd.github.io/Ro-Repo/store/catalog.json")
    }
}
