import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: page

    property var catalog
    property bool narrow: width < 900
    property int sideMargin: narrow ? 24 : 36
    property int contentWidth: Math.max(320, width - sideMargin * 2)

    signal reloadRequested()
    signal appSelected(
        string title,
        string summary,
        string description,
        string category,
        string version,
        string packageName,
        string iconUrl
    )

    Flickable {
        id: flick

        anchors.fill: parent
        clip: true

        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 32

        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        Column {
            id: contentColumn

            width: flick.width
            spacing: 16

            Item {
                width: parent.width
                height: 26
            }

            // ÜST BAR
            Item {
                width: page.contentWidth
                height: page.narrow ? 78 : 70
                x: page.sideMargin

                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: "Ro-Store"
                        color: "#f4f7fb"
                        font.pixelSize: page.narrow ? 28 : 32
                        font.bold: true
                    }

                    Text {
                        text: "Project Ro resmi uygulama mağazası"
                        color: "#9aa4b2"
                        font.pixelSize: 15
                    }
                }

                Row {
                    visible: !page.narrow
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    TextField {
                        id: searchFieldWide
                        width: 310
                        height: 36
                        placeholderText: "Uygulama ara..."
                        text: page.catalog ? page.catalog.searchText : ""

                        onTextChanged: {
                            if (page.catalog) {
                                page.catalog.searchText = text
                            }
                        }
                    }

                    ComboBox {
                        id: categoryBoxWide
                        width: 170
                        height: 36
                        model: page.catalog ? page.catalog.categories : ["Tümü"]

                        onActivated: {
                            if (page.catalog) {
                                page.catalog.categoryFilter = currentText
                            }
                        }
                    }

                    Button {
                        text: "Yenile"
                        width: 90
                        height: 36
                        onClicked: page.reloadRequested()
                    }
                }
            }

            // KÜÇÜK EKRAN KONTROLLERİ
            Item {
                visible: page.narrow
                width: page.contentWidth
                height: visible ? 92 : 0
                x: page.sideMargin

                TextField {
                    id: searchFieldNarrow
                    x: 0
                    y: 0
                    width: parent.width
                    height: 38
                    placeholderText: "Uygulama ara..."
                    text: page.catalog ? page.catalog.searchText : ""

                    onTextChanged: {
                        if (page.catalog) {
                            page.catalog.searchText = text
                        }
                    }
                }

                Row {
                    x: 0
                    y: 50
                    width: parent.width
                    height: 38
                    spacing: 10

                    ComboBox {
                        id: categoryBoxNarrow
                        width: parent.width - 100
                        height: 38
                        model: page.catalog ? page.catalog.categories : ["Tümü"]

                        onActivated: {
                            if (page.catalog) {
                                page.catalog.categoryFilter = currentText
                            }
                        }
                    }

                    Button {
                        text: "Yenile"
                        width: 90
                        height: 38
                        onClicked: page.reloadRequested()
                    }
                }
            }

            // HERO ALANI
            Rectangle {
                width: page.contentWidth
                height: page.narrow ? 285 : 132
                x: page.sideMargin

                radius: 22
                color: "#16202b"
                border.color: "#263445"
                border.width: 1
                antialiasing: true
                clip: true

                // Geniş ekran düzeni
                Item {
                    visible: !page.narrow
                    anchors.fill: parent

                    Rectangle {
                        x: 20
                        y: 30
                        width: 72
                        height: 72
                        radius: 20
                        color: "#243447"
                        antialiasing: true

                        Text {
                            anchors.centerIn: parent
                            text: "R"
                            color: "#79b8ff"
                            font.pixelSize: 36
                            font.bold: true
                        }
                    }

                    Text {
                        x: 112
                        y: 24
                        width: parent.width - 140
                        text: "Ro uygulamalarını keşfet"
                        color: "#ffffff"
                        font.pixelSize: 24
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        x: 112
                        y: 60
                        width: parent.width - 140
                        text: "Ro-Repo içindeki resmi Project Ro uygulamalarını terminal kullanmadan görüntüle, kur, güncelle veya kaldır."
                        color: "#b5c0cc"
                        font.pixelSize: 15
                        elide: Text.ElideRight
                    }

                    Row {
                        x: 112
                        y: 88
                        spacing: 10

                        Rectangle {
                            radius: 10
                            color: "#243447"
                            height: 32
                            width: appCountLabelWide.implicitWidth + 24

                            Text {
                                id: appCountLabelWide
                                anchors.centerIn: parent
                                text: appsGrid.count + " uygulama"
                                color: "#dbeafe"
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }

                        Rectangle {
                            radius: 10
                            color: "#243447"
                            height: 32
                            width: repoLabelWide.implicitWidth + 24

                            Text {
                                id: repoLabelWide
                                anchors.centerIn: parent
                                text: "Kaynak: Ro-Repo"
                                color: "#6ee7b7"
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }
                    }
                }

                // Küçük ekran düzeni
                Column {
                    visible: page.narrow
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Rectangle {
                        width: 78
                        height: 78
                        radius: 22
                        color: "#243447"
                        antialiasing: true
                        anchors.horizontalCenter: parent.horizontalCenter

                        Text {
                            anchors.centerIn: parent
                            text: "R"
                            color: "#79b8ff"
                            font.pixelSize: 38
                            font.bold: true
                        }
                    }

                    Text {
                        width: parent.width
                        text: "Ro uygulamalarını keşfet"
                        color: "#ffffff"
                        font.pixelSize: 23
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: "Ro-Repo içindeki resmi Project Ro uygulamalarını terminal kullanmadan görüntüle, kur, güncelle veya kaldır."
                        color: "#b5c0cc"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        spacing: 10
                        anchors.horizontalCenter: parent.horizontalCenter

                        Rectangle {
                            radius: 10
                            color: "#243447"
                            height: 32
                            width: appCountLabelNarrow.implicitWidth + 24

                            Text {
                                id: appCountLabelNarrow
                                anchors.centerIn: parent
                                text: appsGrid.count + " uygulama"
                                color: "#dbeafe"
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }

                        Rectangle {
                            radius: 10
                            color: "#243447"
                            height: 32
                            width: repoLabelNarrow.implicitWidth + 24

                            Text {
                                id: repoLabelNarrow
                                anchors.centerIn: parent
                                text: "Ro-Repo"
                                color: "#6ee7b7"
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }
                    }
                }
            }

            // BAŞLIK
            Item {
                width: page.contentWidth
                height: 32
                x: page.sideMargin

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Öne çıkan Ro uygulamaları"
                    color: "#ffffff"
                    font.pixelSize: page.narrow ? 18 : 20
                    font.bold: true
                }

                Text {
                    visible: !page.narrow
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.catalog && page.catalog.loading ? "Katalog yükleniyor..." : ""
                    color: "#9aa4b2"
                    font.pixelSize: 13
                }
            }

            Text {
                visible: page.catalog && page.catalog.error.length > 0
                width: page.contentWidth
                x: page.sideMargin
                text: page.catalog ? page.catalog.error : ""
                color: "#ff6b6b"
                font.pixelSize: 15
                wrapMode: Text.WordWrap
            }

            Text {
                visible: page.catalog && !page.catalog.loading && page.catalog.error.length === 0 && appsGrid.count === 0
                width: page.contentWidth
                x: page.sideMargin
                text: "Sonuç bulunamadı."
                color: "#9aa4b2"
                font.pixelSize: 15
            }

            // UYGULAMA KARTLARI
            Item {
                id: gridArea

                width: page.contentWidth
                x: page.sideMargin

                property int cardWidth: 300
                property int cardHeight: 218
                property int gap: 18
                property int columns: page.narrow ? 1 : Math.max(1, Math.floor((width + gap) / (cardWidth + gap)))
                property int rows: Math.max(1, Math.ceil(appsGrid.count / columns))

                height: rows * (cardHeight + gap) + 10

                GridView {
                    id: appsGrid

                    anchors.fill: parent

                    cellWidth: page.narrow ? parent.width : gridArea.cardWidth + gridArea.gap
                    cellHeight: gridArea.cardHeight + gridArea.gap

                    model: page.catalog
                    clip: false
                    interactive: false

                    delegate: Item {
                        width: appsGrid.cellWidth
                        height: appsGrid.cellHeight

                        AppCard {
                            anchors.horizontalCenter: page.narrow ? parent.horizontalCenter : undefined
                            width: page.narrow ? Math.min(300, parent.width) : 300

                            titleText: model.appName
                            summaryText: model.summary
                            descriptionText: model.description
                            categoryText: model.category
                            versionText: model.latestVersion
                            packageText: model.packageName
                            iconUrl: model.iconUrl

                            onDetailRequested: function(title, summary, description, category, version, packageName, iconUrl) {
                                page.appSelected(title, summary, description, category, version, packageName, iconUrl)
                            }
                        }
                    }
                }
            }

            Item {
                width: parent.width
                height: 32
            }
        }
    }
}
