import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: page

    ThemeMetrics { id: metrics }

    SystemPalette {
        id: themePalette
    }

    // System-controlled readable secondary labels (not QPalette.mid).
    readonly property color secondaryText: Qt.rgba(
        themePalette.text.r, themePalette.text.g, themePalette.text.b, 0.76)
    readonly property color secondaryWindowText: Qt.rgba(
        themePalette.windowText.r, themePalette.windowText.g, themePalette.windowText.b, 0.76)

    // Blend the active accent with the foreground to keep meaningful labels
    // colored but comfortable in both light and dark color schemes.
    readonly property color mutedAccent: Qt.rgba(
        themePalette.highlight.r * 0.6 + themePalette.text.r * 0.4,
        themePalette.highlight.g * 0.6 + themePalette.text.g * 0.4,
        themePalette.highlight.b * 0.6 + themePalette.text.b * 0.4,
        1.0)
    property var catalog
    property var packageTransactionManager: null
    property var sourceManager: null

    // Ana sayfa her tekrar aktif olduğunda kartların RPM durumunu yeniler.
    property int packageStatusRefreshSerial: 0

    property bool narrow: width < 900
    property int sideMargin: narrow ? 24 : 36
    property int contentWidth: Math.max(320, width - sideMargin * 2)

    signal reloadRequested()
    signal downloadsRequested()
    signal repositoryActionRequested()

    StackView.onActivated: {
        page.packageStatusRefreshSerial += 1

        console.log(
            "HOME PACKAGE STATUS REFRESH:",
            page.packageStatusRefreshSerial
        )

        if (page.sourceManager) {
            page.sourceManager.refresh()
        }
    }
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
            spacing: metrics.spaceLarge

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
                    spacing: metrics.spaceCompact

                    Text {
                        text: "Ro-Store"
                        color: themePalette.windowText
                        font.pixelSize: page.narrow ? 28 : 32
                        font.bold: true
                    }

                    Text {
                        text: "Project Ro resmi uygulama mağazası"
                        color: page.secondaryWindowText
                        font.pixelSize: 15
                    }
                }

                Row {
                    visible: !page.narrow
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: metrics.spaceMedium

                    TextField {
                        id: searchFieldWide
                        width: 240
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
                        width: 145
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
                        width: 80
                        height: 36
                        onClicked: page.reloadRequested()
                    }

                    Button {
                        text: "Yüklemeler"
                        width: 110
                        height: 36
                        onClicked: page.downloadsRequested()
                    }
                }
            }

            // KÜÇÜK EKRAN KONTROLLERİ
            Item {
                visible: page.narrow
                width: page.contentWidth
                height: visible ? 142 : 0
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
                    spacing: metrics.spaceMedium

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

                Button {
                    x: 0
                    y: 100
                    width: parent.width
                    height: 38

                    text: "Yüklemeler"
                    onClicked: page.downloadsRequested()
                }
            }

            // RO-ASD REPOSITORY DURUMU
            Rectangle {
                visible: page.sourceManager
                         && !page.sourceManager.checking
                         && !page.sourceManager.roAsdReady

                width: page.contentWidth
                height: visible
                        ? (
                            page.narrow
                            ? (
                                page.sourceManager
                                && page.sourceManager.repositoryActionError.length > 0
                                ? 190
                                : 150
                              )
                            : (
                                page.sourceManager
                                && page.sourceManager.repositoryActionError.length > 0
                                ? 132
                                : 104
                              )
                          )
                        : 0
                x: page.sideMargin

                radius: metrics.radiusPanel
                color: themePalette.window
                border.color: page.secondaryText
                border.width: 1
                antialiasing: true

                Column {
                    visible: page.narrow
                    anchors.fill: parent
                    anchors.margins: metrics.spaceLarge
                    spacing: metrics.spaceMedium

                    Text {
                        width: parent.width
                        text: "Ro-ASD Uygulama Deposu"
                        color: themePalette.windowText
                        font.pixelSize: 17
                        font.bold: true
                    }

                    Text {
                        width: parent.width
                        text: page.sourceManager
                              ? page.sourceManager.roAsdStatusText
                              : ""
                        color: themePalette.text
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        visible: page.sourceManager
                                 && page.sourceManager.repositoryActionError.length > 0

                        width: parent.width

                        text: page.sourceManager
                              ? page.sourceManager.repositoryActionError
                              : ""

                        color: themePalette.text
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        visible: page.sourceManager
                                 && page.sourceManager.roAsdActionAvailable

                        width: parent.width
                        height: 36

                        text: page.sourceManager
                              ? (
                                  page.sourceManager.repositoryActionRunning
                                  ? "İşlem yapılıyor..."
                                  : page.sourceManager.roAsdActionText
                                )
                              : ""

                        enabled: page.sourceManager
                                 && !page.sourceManager.repositoryActionRunning

                        onClicked: {
                            if (!page.sourceManager)
                                return

                            if (page.sourceManager.roAsdActionText === "Tekrar Dene") {
                                page.sourceManager.refresh()
                                return
                            }

                            page.repositoryActionRequested()
                        }
                    }
                }

                Item {
                    visible: !page.narrow
                    anchors.fill: parent

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: metrics.spaceExtraLarge
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                               - 40
                               - (repoActionWide.visible ? 210 : 0)
                        spacing: metrics.spaceNormal

                        Text {
                            width: parent.width
                            text: "Ro-ASD Uygulama Deposu"
                            color: themePalette.windowText
                            font.pixelSize: 17
                            font.bold: true
                        }

                        Text {
                            width: parent.width
                            text: page.sourceManager
                                  ? page.sourceManager.roAsdStatusText
                                  : ""
                            color: themePalette.text
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: page.sourceManager
                                     && page.sourceManager.repositoryActionError.length > 0

                            width: parent.width

                            text: page.sourceManager
                                  ? page.sourceManager.repositoryActionError
                                  : ""

                            color: themePalette.text
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }

                    Button {
                        id: repoActionWide

                        visible: page.sourceManager
                                 && page.sourceManager.roAsdActionAvailable

                        anchors.right: parent.right
                        anchors.rightMargin: metrics.spaceExtraLarge
                        anchors.verticalCenter: parent.verticalCenter

                        width: 190
                        height: 38

                        text: page.sourceManager
                              ? (
                                  page.sourceManager.repositoryActionRunning
                                  ? "İşlem yapılıyor..."
                                  : page.sourceManager.roAsdActionText
                                )
                              : ""

                        enabled: page.sourceManager
                                 && !page.sourceManager.repositoryActionRunning

                        onClicked: {
                            if (!page.sourceManager)
                                return

                            if (page.sourceManager.roAsdActionText === "Tekrar Dene") {
                                page.sourceManager.refresh()
                                return
                            }

                            page.repositoryActionRequested()
                        }
                    }
                }
            }

            // HERO ALANI
            Rectangle {
                width: page.contentWidth
                height: page.narrow ? 285 : 132
                x: page.sideMargin

                radius: metrics.radiusCard
                color: themePalette.base
                border.color: page.secondaryText
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
                        radius: metrics.radiusPanel
                        color: themePalette.button
                        antialiasing: true

                        Text {
                            anchors.centerIn: parent
                            text: "R"
                            color: themePalette.highlight
                            font.pixelSize: 36
                            font.bold: true
                        }
                    }

                    Text {
                        x: 112
                        y: 24
                        width: parent.width - 140
                        text: "Ro uygulamalarını keşfet"
                        color: themePalette.text
                        font.pixelSize: 24
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        x: 112
                        y: 60
                        width: parent.width - 140
                        text: "Ro-Repo içindeki resmi Project Ro uygulamalarını terminal kullanmadan görüntüle, kur, güncelle veya kaldır."
                        color: themePalette.text
                        font.pixelSize: 15
                        elide: Text.ElideRight
                    }

                    Row {
                        x: 112
                        y: 88
                        spacing: metrics.spaceMedium

                        Rectangle {
                            radius: metrics.radiusControl
                            color: themePalette.button
                            border.color: themePalette.mid
                            border.width: 1
                            height: 32
                            width: appCountLabelWide.implicitWidth + 24

                            Text {
                                id: appCountLabelWide
                                anchors.centerIn: parent
                                text: appsGrid.count + " uygulama"
                                color: themePalette.buttonText
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }

                        Rectangle {
                            radius: metrics.radiusControl
                            color: themePalette.button
                            border.color: themePalette.mid
                            border.width: 1
                            height: 32
                            width: repoLabelWide.implicitWidth + 24

                            Text {
                                id: repoLabelWide
                                anchors.centerIn: parent
                                text: "Kaynak: Ro-Repo"
                                color: page.mutedAccent
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
                    anchors.margins: metrics.spaceExtraLarge
                    spacing: metrics.spaceMedium

                    Rectangle {
                        width: 78
                        height: 78
                        radius: metrics.radiusCard
                        color: themePalette.button
                        antialiasing: true
                        anchors.horizontalCenter: parent.horizontalCenter

                        Text {
                            anchors.centerIn: parent
                            text: "R"
                            color: themePalette.highlight
                            font.pixelSize: 38
                            font.bold: true
                        }
                    }

                    Text {
                        width: parent.width
                        text: "Ro uygulamalarını keşfet"
                        color: themePalette.text
                        font.pixelSize: 23
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: "Ro-Repo içindeki resmi Project Ro uygulamalarını terminal kullanmadan görüntüle, kur, güncelle veya kaldır."
                        color: themePalette.text
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        spacing: metrics.spaceMedium
                        anchors.horizontalCenter: parent.horizontalCenter

                        Rectangle {
                            radius: metrics.radiusControl
                            color: themePalette.button
                            border.color: themePalette.mid
                            border.width: 1
                            height: 32
                            width: appCountLabelNarrow.implicitWidth + 24

                            Text {
                                id: appCountLabelNarrow
                                anchors.centerIn: parent
                                text: appsGrid.count + " uygulama"
                                color: themePalette.buttonText
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }

                        Rectangle {
                            radius: metrics.radiusControl
                            color: themePalette.button
                            border.color: themePalette.mid
                            border.width: 1
                            height: 32
                            width: repoLabelNarrow.implicitWidth + 24

                            Text {
                                id: repoLabelNarrow
                                anchors.centerIn: parent
                                text: "Ro-Repo"
                                color: page.mutedAccent
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
                    color: themePalette.text
                    font.pixelSize: page.narrow ? 18 : 20
                    font.bold: true
                }

                Text {
                    visible: !page.narrow
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.catalog && page.catalog.loading ? "Katalog yükleniyor..." : ""
                    color: page.secondaryText
                    font.pixelSize: 13
                }
            }

            Text {
                visible: page.catalog && page.catalog.error.length > 0
                width: page.contentWidth
                x: page.sideMargin
                text: page.catalog ? page.catalog.error : ""
                color: themePalette.text
                font.pixelSize: 15
                wrapMode: Text.WordWrap
            }

            Text {
                visible: page.catalog && !page.catalog.loading && page.catalog.error.length === 0 && appsGrid.count === 0
                width: page.contentWidth
                x: page.sideMargin
                text: "Sonuç bulunamadı."
                color: page.secondaryText
                font.pixelSize: 15
            }

            // UYGULAMA KARTLARI
            Item {
                id: gridArea

                width: page.contentWidth
                x: page.sideMargin

                property int cardWidth: 300
                // Keep the grid cell in sync with the theme-sized AppCard.
                property int cardHeight: Math.ceil(metrics.applicationCardHeight)
                property int gap: Math.ceil(metrics.spaceExtraLarge)
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
                            packageTransactionManager:
                                page.packageTransactionManager

                            statusRefreshSerial:
                                page.packageStatusRefreshSerial

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
