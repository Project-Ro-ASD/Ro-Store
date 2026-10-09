import QtQuick
import QtQuick.Controls
import org.kde.kirigami.platform as Platform
import "../components"
import RoStore 1.0

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
    property string appName: ""
    property string summaryText: ""
    property string descriptionText: ""
    property string categoryText: ""
    property string versionText: ""
    property string packageName: ""
    property string iconUrl: ""

    property var packageTransactionManager: null
    property var repositorySourceManager: null
    property var trackedTransaction: null

    property bool transactionRunning:
        trackedTransaction !== null
        && trackedTransaction.state !== PackageTransaction.Finished
        && trackedTransaction.state !== PackageTransaction.Failed
        && trackedTransaction.state !== PackageTransaction.Cancelled

    property int transactionProgress:
        trackedTransaction !== null
        ? trackedTransaction.progress
        : 0

    // Kurulum ve güncelleme için Ro-ASD kaynağının hazır
    // olması gerekir. Kaldırma işlemi repository gerektirmez.
    property bool repositoryRequired:
        !packageStatus.installed
        || packageStatus.updateAvailable

    property bool repositoryBlocked:
        repositoryRequired
        && (
            repositorySourceManager === null
            || !repositorySourceManager.roAsdReady
        )

    property bool repositoryActionAvailable:
        repositorySourceManager !== null
        && repositorySourceManager.roAsdActionAvailable
        && !repositorySourceManager.repositoryActionRunning

    property string repositoryBlockedText: {
        if (!repositoryBlocked)
            return ""

        if (!repositorySourceManager)
            return "Paket kaynağı kullanılamıyor."

        if (repositorySourceManager.repositoryActionError.length > 0)
            return repositorySourceManager.repositoryActionError

        return repositorySourceManager.roAsdStatusText
    }

    property string primaryActionText: {
        if (transactionRunning)
            return narrow ? "İşlem..." : "İşlem sürüyor..."

        if (repositoryBlocked) {
            if (!repositorySourceManager)
                return "Depo hazır değil"

            if (repositorySourceManager.repositoryActionRunning)
                return "Depo hazırlanıyor..."

            if (repositorySourceManager.roAsdActionAvailable)
                return repositorySourceManager.roAsdActionText

            if (repositorySourceManager.roAsdState
                    === SourceManager.InvalidConfiguration)
                return "Depo yapılandırması geçersiz"

            if (repositorySourceManager.roAsdState
                    === SourceManager.Unsupported)
                return "Sistem desteklenmiyor"

            return "Depo hazır değil"
        }

        if (!packageStatus.installed)
            return "Kur"

        if (packageStatus.updateAvailable)
            return "Güncelle"

        return "Kaldır"
    }

    property string transactionPhaseText: {
        if (trackedTransaction === null)
            return ""

        switch (trackedTransaction.state) {
        case PackageTransaction.Queued:
            return "Sırada"
        case PackageTransaction.Resolving:
            return "Hazırlanıyor"
        case PackageTransaction.Ready:
            return "Hazır"
        case PackageTransaction.Downloading:
            return "İndiriliyor"
        case PackageTransaction.Running:
            if (trackedTransaction.operation === PackageTransaction.Remove)
                return "Kaldırılıyor"

            if (trackedTransaction.operation === PackageTransaction.Upgrade)
                return "Güncelleniyor"

            return "Kuruluyor"
        case PackageTransaction.Finished:
            return "Tamamlandı"
        case PackageTransaction.Failed:
            return "Başarısız"
        case PackageTransaction.Cancelled:
            return "İptal edildi"
        }

        return "İşleniyor"
    }

    property string transactionTechnicalText: {
        if (trackedTransaction === null)
            return "Henüz işlem bilgisi yok."

        var lines = []

        lines.push("Paket: " + trackedTransaction.packageName)
        lines.push("Durum: " + transactionPhaseText)
        lines.push("İlerleme: %" + transactionProgress)

        if (trackedTransaction.totalBytes > 0) {
            lines.push(
                "İndirilen: "
                + trackedTransaction.downloadedBytes
                + " / "
                + trackedTransaction.totalBytes
                + " bayt"
            )
        }

        if (trackedTransaction.errorMessage.length > 0)
            lines.push("Hata: " + trackedTransaction.errorMessage)

        return lines.join("\n")
    }

    function startInstallTransaction() {
        if (!packageTransactionManager || transactionRunning)
            return

        page.logsExpanded = false
        page.lastActionMessage = ""

        page.trackedTransaction =
            packageTransactionManager.enqueueInstall(
                page.packageName
            )
    }

    function startUpgradeTransaction() {
        if (!packageTransactionManager || transactionRunning)
            return

        page.logsExpanded = false
        page.lastActionMessage = ""

        page.trackedTransaction =
            packageTransactionManager.enqueueUpgrade(
                page.packageName
            )
    }

    function startRemoveTransaction() {
        if (!packageTransactionManager || transactionRunning)
            return

        page.logsExpanded = false
        page.lastActionMessage = ""

        page.trackedTransaction =
            packageTransactionManager.enqueueRemove(
                page.packageName
            )
    }

    property bool narrow: width < Math.max(760, Math.ceil(760 * metrics.fontScale))
    property int sideMargin: Math.min(narrow ? 22 : 36,
                                      Math.max(8, Math.floor(width / 12)))
    property int contentWidth: Math.max(0, width - sideMargin * 2)

    property bool logsExpanded: false
    property string lastActionMessage: ""
    property bool lastActionSuccess: true

    property string displayStatusText:
        page.transactionRunning
        ? page.transactionPhaseText
        : lastActionMessage.length > 0
            ? lastActionMessage
            : page.repositoryBlocked
                ? page.repositoryBlockedText
                : packageStatus.statusText

    // Status text is informational, not a selected control. Keep the
    // system accent for actions and progress; use readable neutral text here.
    property color displayStatusColor:
        page.transactionRunning
        ? page.mutedAccent
        : lastActionMessage.length > 0
            ? (lastActionSuccess ? page.mutedAccent : themePalette.text)
            : page.repositoryBlocked
                ? themePalette.text
                : page.mutedAccent

    signal backRequested()
    signal downloadsRequested()

    PackageStatus {
        id: packageStatus
    }

    AppLauncher {
        id: appLauncher
    }

    Connections {
        target: page.trackedTransaction

        function onStateChanged() {
            if (!page.trackedTransaction)
                return

            if (page.trackedTransaction.state === PackageTransaction.Finished) {
                if (page.trackedTransaction.operation === PackageTransaction.Remove) {
                    page.lastActionMessage = "Uygulama başarıyla kaldırıldı."
                } else if (page.trackedTransaction.operation === PackageTransaction.Upgrade) {
                    page.lastActionMessage = "Uygulama başarıyla güncellendi."
                } else {
                    page.lastActionMessage = "Uygulama başarıyla kuruldu."
                }

                page.lastActionSuccess = true
                page.logsExpanded = false

                packageStatus.checkInstalled(
                    page.packageName,
                    page.versionText
                )

            } else if (page.trackedTransaction.state === PackageTransaction.Failed) {
                page.lastActionMessage =
                    page.trackedTransaction.errorMessage.length > 0
                    ? page.trackedTransaction.errorMessage
                    : "İşlem başarısız."

                page.lastActionSuccess = false
                page.logsExpanded = false

                packageStatus.checkInstalled(
                    page.packageName,
                    page.versionText
                )

            } else if (page.trackedTransaction.state === PackageTransaction.Cancelled) {
                page.lastActionMessage = "İşlem iptal edildi."
                page.lastActionSuccess = false
                page.logsExpanded = false

                packageStatus.checkInstalled(
                    page.packageName,
                    page.versionText
                )
            }
        }
    }

    Component.onCompleted: {
        if (repositorySourceManager)
            repositorySourceManager.refresh()

        if (packageTransactionManager) {
            var pending =
                packageTransactionManager.pendingTransaction(
                    page.packageName
                )

            if (pending)
                page.trackedTransaction = pending
        }

        packageStatus.checkInstalled(
            page.packageName,
            page.versionText
        )
    }

    // ÜST BAR
    Rectangle {
        id: topBar

        x: page.sideMargin
        y: 20
        width: page.contentWidth
        height: Math.max(44, Math.max(backButton.height, downloadsButton.height) + metrics.spaceNormal)
        color: "transparent"

        ThemedIconButton {
            id: backButton
            text: "Geri"
            themedIconName: "go-previous"
            icon.width: 16
            icon.height: 16
            width: Math.max(95, implicitWidth + metrics.spaceMedium)
            height: Math.max(36, implicitHeight)
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            enabled: !page.transactionRunning
            onClicked: page.backRequested()
        }

        Text {
            visible: !page.narrow
            text: "Ro-Store"
            color: page.secondaryText
            font.family: metrics.systemFont.family
            font.pixelSize: metrics.fontBody
            anchors.right: downloadsButton.left
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
        }

        ThemedIconButton {
            id: downloadsButton

            text: "Yüklemeler"
            themedIconName: "folder-download"
            icon.width: 16
            icon.height: 16
            width: Math.max(128, implicitWidth + metrics.spaceMedium)
            height: Math.max(36, implicitHeight)

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            onClicked: page.downloadsRequested()
        }
    }

    // İÇERİK ALANI
    Flickable {
        id: flick

        x: 0
        y: topBar.y + topBar.height + 12
        width: parent.width
        height: bottomBar.y - y - 12
        clip: true

        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 30
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        Column {
            id: contentColumn

            width: flick.width
            spacing: metrics.spaceExtraLarge

            // UYGULAMA BAŞLIK KARTI
            Rectangle {
                width: page.contentWidth
                height: page.narrow
                        ? Math.max(300, detailHeaderNarrow.implicitHeight
                                       + metrics.spaceExtraLarge * 2)
                        : Math.max(180, detailBadgesWide.y + detailBadgesWide.height
                                       + metrics.spaceNormal)
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
                        x: 26
                        y: 42
                        width: 96
                        height: 96
                        radius: metrics.radiusCard
                        color: themePalette.button
                        clip: true
                        antialiasing: true

                        Image {
                            id: detailIconWide
                            anchors.centerIn: parent
                            width: 64
                            height: 64
                            source: page.iconUrl
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            visible: page.iconUrl.length > 0 && status === Image.Ready
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !detailIconWide.visible
                            text: page.appName.length > 0 ? page.appName[0].toUpperCase() : "R"
                            color: themePalette.text
                            font.family: metrics.systemFont.family
                            font.pixelSize: metrics.fontPx(38)
                            font.bold: true
                        }
                    }

                    Text {
                        id: detailTitleWide
                        x: 148
                        y: 34
                        width: parent.width - 180
                        text: page.appName
                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontPx(32)
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        id: detailSummaryWide
                        x: 148
                        y: detailTitleWide.y + detailTitleWide.height + metrics.spaceCompact
                        width: parent.width - 180
                        text: page.summaryText
                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontPx(16)
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Row {
                        id: detailBadgesWide
                        x: 148
                        y: detailSummaryWide.y + detailSummaryWide.height + metrics.spaceMedium
                        spacing: metrics.spaceMedium

                        Rectangle {
                            radius: metrics.radiusControl
                            color: themePalette.button
                            border.color: themePalette.mid
                            border.width: 1
                            height: Math.max(32, categoryLabelWide.implicitHeight + 10)
                            width: categoryLabelWide.implicitWidth + 24

                            Text {
                                id: categoryLabelWide
                                anchors.centerIn: parent
                                text: page.categoryText
                                color: page.mutedAccent
                                font.family: metrics.systemFont.family
                                font.pixelSize: metrics.fontSmall
                            }
                        }

                        Rectangle {
                            radius: metrics.radiusControl
                            color: themePalette.button
                            border.color: themePalette.mid
                            border.width: 1
                            height: Math.max(32, versionLabelWide.implicitHeight + 10)
                            width: versionLabelWide.implicitWidth + 24

                            Text {
                                id: versionLabelWide
                                anchors.centerIn: parent
                                text: "Sürüm " + page.versionText
                                color: themePalette.text
                                font.family: metrics.systemFont.family
                                font.pixelSize: metrics.fontSmall
                            }
                        }
                    }
                }

                // Küçük ekran düzeni
                Column {
                    id: detailHeaderNarrow
                    visible: page.narrow
                    anchors.fill: parent
                    anchors.margins: metrics.spaceExtraLarge
                    spacing: metrics.spaceMedium

                    Rectangle {
                        width: 92
                        height: 92
                        radius: metrics.radiusCard
                        color: themePalette.button
                        clip: true
                        antialiasing: true
                        anchors.horizontalCenter: parent.horizontalCenter

                        Image {
                            id: detailIconNarrow
                            anchors.centerIn: parent
                            width: 62
                            height: 62
                            source: page.iconUrl
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            visible: page.iconUrl.length > 0 && status === Image.Ready
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !detailIconNarrow.visible
                            text: page.appName.length > 0 ? page.appName[0].toUpperCase() : "R"
                            color: themePalette.text
                            font.family: metrics.systemFont.family
                            font.pixelSize: metrics.fontPx(36)
                            font.bold: true
                        }
                    }

                    Text {
                        width: parent.width
                        text: page.appName
                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontPx(28)
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: page.summaryText
                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBodyLarge
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
                            height: Math.max(32, categoryLabelNarrow.implicitHeight + 10)
                            width: categoryLabelNarrow.implicitWidth + 24

                            Text {
                                id: categoryLabelNarrow
                                anchors.centerIn: parent
                                text: page.categoryText
                                color: page.mutedAccent
                                font.family: metrics.systemFont.family
                                font.pixelSize: metrics.fontSmall
                            }
                        }

                        Rectangle {
                            radius: metrics.radiusControl
                            color: themePalette.button
                            border.color: themePalette.mid
                            border.width: 1
                            height: Math.max(32, versionLabelNarrow.implicitHeight + 10)
                            width: versionLabelNarrow.implicitWidth + 24

                            Text {
                                id: versionLabelNarrow
                                anchors.centerIn: parent
                                text: "Sürüm " + page.versionText
                                color: themePalette.text
                                font.family: metrics.systemFont.family
                                font.pixelSize: metrics.fontSmall
                            }
                        }
                    }
                }
            }

            // AÇIKLAMA VE PAKET BİLGİLERİ
            Rectangle {
                width: page.contentWidth
                x: page.sideMargin
                height: infoColumn.implicitHeight + 48

                radius: metrics.radiusPanel
                color: themePalette.base
                border.color: page.secondaryText
                border.width: 1
                antialiasing: true

                Column {
                    id: infoColumn

                    x: 24
                    y: 24
                    width: parent.width - 48
                    spacing: metrics.spaceLarge

                    Text {
                        width: parent.width
                        text: "Açıklama"
                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontPageTitle
                        font.bold: true
                    }

                    Text {
                        width: parent.width
                        text: page.descriptionText
                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBodyLarge
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: page.secondaryText
                    }

                    Text {
                        width: parent.width
                        text: "Paket bilgileri"
                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontTitle
                        font.bold: true
                    }

                    Text {
                        width: parent.width
                        text: "Paket adı: " + page.packageName
                        color: page.secondaryText
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: "Kaynak: Ro-Repo"
                        color: page.secondaryText
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                    }

                    Text {
                        width: parent.width
                        text: "Kurulu sürüm: " + (packageStatus.installedVersion.length > 0 ? packageStatus.installedVersion : "-")
                        color: page.secondaryText
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                    }

                    Text {
                        width: parent.width
                        text: "Repo sürümü: " + (page.versionText.length > 0 ? page.versionText : "-")
                        color: page.secondaryText
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                    }

                    Text {
                        width: parent.width
                        text: page.displayStatusText
                        color: page.displayStatusColor
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: packageStatus.installed
                              ? "Bu uygulama sistemde yüklü."
                              : "Kur/Güncelle/Kaldır işlemleri DNF5 üzerinden yapılır."
                        color: page.secondaryText
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontSmall
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // TEKNİK LOG AYRINTILARI - varsayılan kapalı
            Item {
                visible: page.trackedTransaction !== null

                width: page.contentWidth
                height: visible
                        ? (page.logsExpanded
                           ? Math.max(page.narrow ? 260 : 285,
                                      logToggleHeader.height + 200)
                           : logToggleHeader.height)
                        : 0
                x: page.sideMargin

                Behavior on height {
                    NumberAnimation {
                        duration: 150
                    }
                }

                Rectangle {
                    id: logToggleHeader
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: Math.max(42, metrics.fontPx(42))
                    radius: metrics.radiusControl
                    color: logToggleMouse.containsMouse ? themePalette.button : "transparent"
                    border.color: logToggleMouse.containsMouse ? themePalette.highlight : "transparent"
                    border.width: 1
                    antialiasing: true

                    Row {
                        id: logToggleRow
                        anchors.left: parent.left
                        anchors.leftMargin: metrics.spaceNormal
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, parent.width - metrics.spaceNormal * 2)
                        spacing: metrics.spaceNormal

                        Text {
                            id: logToggleArrow
                            text: page.logsExpanded ? "⌄" : "›"
                            color: page.secondaryText
                            font.family: metrics.systemFont.family
                            font.pixelSize: metrics.fontTitle
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            width: Math.max(0, logToggleRow.width
                                        - logToggleArrow.implicitWidth
                                        - logToggleRow.spacing
                                        - (logToggleStatus.visible
                                           ? logToggleStatus.implicitWidth + logToggleRow.spacing
                                           : 0))
                            elide: Text.ElideRight
                            text: page.logsExpanded
                                  ? "Teknik işlem günlüklerini gizle"
                                  : "Teknik işlem günlüklerini göster"
                            color: page.secondaryText
                            font.family: metrics.systemFont.family
                            font.pixelSize: metrics.fontSmall
                            font.italic: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            id: logToggleStatus
                            visible: !page.narrow && logToggleRow.width >= 640
                            text: page.transactionRunning ? "• işlem devam ediyor" : "• ayrıntılar hazır"
                            color: page.transactionRunning ? themePalette.highlight : page.secondaryText
                            font.family: metrics.systemFont.family
                            font.pixelSize: metrics.fontCaption
                            font.italic: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: logToggleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: page.logsExpanded = !page.logsExpanded
                    }
                }

                Rectangle {
                    visible: page.logsExpanded
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: logToggleHeader.height + 6
                    height: Math.max(0, parent.height - logToggleHeader.height - 6)
                    radius: metrics.radiusInner
                    color: themePalette.base
                    border.color: page.secondaryText
                    border.width: 1
                    antialiasing: true
                    clip: true

                    Text {
                        x: 16
                        y: 12
                        width: Math.max(0, parent.width - 32)
                        elide: Text.ElideRight
                        text: "Teknik İşlem Günlüğü"
                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBody
                        font.bold: true
                    }

                    Text {
                        visible: !page.narrow && parent.width >= 640
                        anchors.right: parent.right
                        anchors.rightMargin: metrics.spaceLarge
                        y: 14
                        text: "DNF5 işlem bilgisi"
                        color: page.secondaryText
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontCaption
                    }

                    Rectangle {
                        x: 16
                        y: 38
                        width: parent.width - 32
                        height: 1
                        color: themePalette.dark
                    }

                    ScrollView {
                        x: 12
                        y: 48
                        width: parent.width - 24
                        height: parent.height - 60
                        clip: true

                        TextArea {
                            text: page.transactionTechnicalText

                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.Wrap
                            font.family: Platform.Theme.fixedWidthFont.family
                            font.pixelSize: metrics.fontSmall
                            color: themePalette.text

                            background: Rectangle {
                                color: themePalette.base
                                border.color: "transparent"
                            }
                        }
                    }
                }
            }

            Item {
                width: parent.width
                height: 24
            }
        }
    }

    // ALT SABİT BUTON BAR
    Rectangle {
        id: bottomBar

        x: page.sideMargin
        y: parent.height - height - 20
        width: page.contentWidth
        height: page.narrow
                ? Math.max(page.transactionRunning ? 132 : 104,
                           (page.transactionRunning ? 82 : 54)
                           + Math.max(38, metrics.fontPx(38)) + 12)
                : Math.max(62, metrics.fontPx(38) + metrics.spaceNormal * 2)

        radius: metrics.radiusInner
        color: themePalette.base
        border.color: themePalette.button
        border.width: 1
        antialiasing: true

        // Geniş ekran alt bar
        Item {
            visible: !page.narrow
            anchors.fill: parent

            ThemedActionButton {
                text: "Durumu Yenile"
                width: 130
                height: 38
                anchors.left: parent.left
                anchors.leftMargin: metrics.spaceMedium
                anchors.verticalCenter: parent.verticalCenter
                enabled: !packageStatus.checking && !page.transactionRunning
                onClicked: packageStatus.checkInstalled(page.packageName, page.versionText)
            }

            Text {
                x: 155
                width: parent.width - (page.transactionRunning ? 665 : 455)
                anchors.verticalCenter: parent.verticalCenter
                text: page.displayStatusText
                color: page.displayStatusColor
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontSmall
                elide: Text.ElideRight
            }

            Rectangle {
                visible: page.transactionRunning
                width: 185
                height: 30
                anchors.right: parent.right
                anchors.rightMargin: 282
                anchors.verticalCenter: parent.verticalCenter

                radius: metrics.radiusControl
                color: themePalette.button
                border.color: themePalette.mid
                border.width: 1
                clip: true
                antialiasing: true

                Rectangle {
                    width: Math.max(0, parent.width * page.transactionProgress / 100)
                    height: parent.height
                    radius: metrics.radiusControl
                    color: themePalette.highlight
                    antialiasing: true

                    Behavior on width {
                        NumberAnimation {
                            duration: 120
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: page.transactionPhaseText + "  %" + page.transactionProgress
                    color: page.transactionProgress >= 65
                           ? themePalette.highlightedText
                           : themePalette.buttonText
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontCaption
                    font.bold: true
                }
            }

            ThemedActionButton {
                accented: true
                visible: packageStatus.installed && !page.transactionRunning
                text: "Çalıştır"
                width: 120
                height: 38
                anchors.right: parent.right
                anchors.rightMargin: 154
                anchors.verticalCenter: parent.verticalCenter

                onClicked: {
                    appLauncher.launch(page.packageName)
                    page.lastActionMessage = appLauncher.statusText
                    page.lastActionSuccess = true
                }
            }

            ThemedActionButton {
                // Install/update actions are prominent, removal stays outlined.
                accented: page.repositoryBlocked
                          || !packageStatus.installed
                          || packageStatus.updateAvailable
                outlined: !accented
                text: page.primaryActionText

                width: page.repositoryBlocked ? 210 : 130
                height: 38
                anchors.right: parent.right
                anchors.rightMargin: metrics.spaceMedium
                anchors.verticalCenter: parent.verticalCenter

                enabled:
                    !packageStatus.checking
                    && !page.transactionRunning
                    && (
                        !page.repositoryBlocked
                        || page.repositoryActionAvailable
                    )

                onClicked: {
                    page.logsExpanded = false
                    page.lastActionMessage = ""

                    // Repo hazır değilse DNF5 transaction oluşturma.
                    // Uygun repo aksiyonu varsa önce onu çalıştır.
                    if (page.repositoryBlocked) {
                        if (page.repositoryActionAvailable)
                            page.repositorySourceManager.executeRoAsdAction()

                        return
                    }

                    if (!packageStatus.installed) {
                        page.startInstallTransaction()
                    } else if (packageStatus.updateAvailable) {
                        page.startUpgradeTransaction()
                    } else {
                        removeConfirmDialog.open()
                    }
                }
            }
        }

        // Küçük ekran alt bar
        Item {
            visible: page.narrow
            anchors.fill: parent

            Text {
                x: 12
                y: 10
                width: parent.width - 24
                text: page.displayStatusText
                color: page.displayStatusColor
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontSmall
                elide: Text.ElideRight
            }

            Rectangle {
                visible: page.transactionRunning
                x: 12
                y: 38
                width: parent.width - 24
                height: 28

                radius: metrics.radiusControl
                color: themePalette.button
                border.color: themePalette.mid
                border.width: 1
                clip: true
                antialiasing: true

                Rectangle {
                    width: Math.max(0, parent.width * page.transactionProgress / 100)
                    height: parent.height
                    radius: metrics.radiusControl
                    color: themePalette.highlight
                    antialiasing: true

                    Behavior on width {
                        NumberAnimation {
                            duration: 120
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: page.transactionPhaseText + "  %" + page.transactionProgress
                    color: page.transactionProgress >= 65
                           ? themePalette.highlightedText
                           : themePalette.buttonText
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontCaption
                    font.bold: true
                }
            }

            ThemedActionButton {
                accented: true
                visible: packageStatus.installed && !page.transactionRunning
                text: "Çalıştır"
                x: 12
                y: 54
                width: (parent.width - 48) / 3
                height: Math.max(38, metrics.fontPx(38))

                onClicked: {
                    appLauncher.launch(page.packageName)
                    page.lastActionMessage = appLauncher.statusText
                    page.lastActionSuccess = true
                }
            }

            ThemedActionButton {
                text: "Durumu Yenile"
                x: packageStatus.installed && !page.transactionRunning ? 12 + ((parent.width - 48) / 3) + 12 : 12
                y: page.transactionRunning ? 82 : 54
                width: packageStatus.installed && !page.transactionRunning ? (parent.width - 48) / 3 : (parent.width - 36) / 2
                height: Math.max(38, metrics.fontPx(38))
                enabled: !packageStatus.checking && !page.transactionRunning
                onClicked: packageStatus.checkInstalled(page.packageName, page.versionText)
            }

            ThemedActionButton {
                // Install/update actions are prominent, removal stays outlined.
                accented: page.repositoryBlocked
                          || !packageStatus.installed
                          || packageStatus.updateAvailable
                outlined: !accented
                text: page.primaryActionText

                x: packageStatus.installed && !page.transactionRunning
                   ? 36 + 2 * ((parent.width - 48) / 3)
                   : 24 + (parent.width - 36) / 2

                y: page.transactionRunning ? 82 : 54

                width: packageStatus.installed && !page.transactionRunning
                       ? (parent.width - 48) / 3
                       : (parent.width - 36) / 2

                height: Math.max(38, metrics.fontPx(38))

                enabled:
                    !packageStatus.checking
                    && !page.transactionRunning
                    && (
                        !page.repositoryBlocked
                        || page.repositoryActionAvailable
                    )

                onClicked: {
                    page.logsExpanded = false
                    page.lastActionMessage = ""

                    if (page.repositoryBlocked) {
                        if (page.repositoryActionAvailable)
                            page.repositorySourceManager.executeRoAsdAction()

                        return
                    }

                    if (!packageStatus.installed) {
                        page.startInstallTransaction()
                    } else if (packageStatus.updateAvailable) {
                        page.startUpgradeTransaction()
                    } else {
                        removeConfirmDialog.open()
                    }
                }
            }
        }
    }

    // KALDIRMA ONAY PENCERESİ
    FocusScope {
        id: removeConfirmDialog

        anchors.fill: parent
        visible: false
        focus: visible
        z: 9999

        function open() {
            removeDialogScroll.contentY = 0
            visible = true
            cancelRemoveButton.forceActiveFocus(Qt.TabFocusReason)
        }

        function close() {
            visible = false
        }

        Shortcut {
            sequence: "Esc"
            context: Qt.WindowShortcut
            enabled: removeConfirmDialog.visible
            onActivated: removeConfirmDialog.close()
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(themePalette.shadow.r, themePalette.shadow.g, themePalette.shadow.b, 0.6)

            MouseArea {
                anchors.fill: parent
                onClicked: removeConfirmDialog.close()
            }
        }

        Rectangle {
            id: removePanel

            // Keep the dialog inside the available window. When font size or
            // window scaling makes its content taller, allow scrolling to
            // reach both confirmation actions rather than clipping them.
            width: Math.max(0, Math.min(520, page.width - metrics.spaceLarge * 2))
            height: Math.max(0, Math.min(page.height - metrics.spaceLarge * 2,
                                      removeDialogContent.implicitHeight
                                      + metrics.spaceLarge * 2))
            anchors.centerIn: parent

            radius: metrics.radiusCard
            color: themePalette.base
            border.color: page.secondaryText
            border.width: 1
            clip: true
            antialiasing: true
            layer.enabled: true
            layer.smooth: true

            MouseArea {
                anchors.fill: parent
                onClicked: mouse.accepted = true
            }

            Flickable {
                id: removeDialogScroll
                anchors.fill: parent
                anchors.margins: metrics.spaceLarge
                contentWidth: width
                contentHeight: removeDialogContent.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                clip: true

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                Column {
                    id: removeDialogContent
                    width: removeDialogScroll.width
                    spacing: metrics.spaceMedium

                    Row {
                        width: parent.width
                        height: Math.max(removeAlertIcon.height, removeHeadingColumn.implicitHeight)
                        spacing: metrics.spaceMedium

                        Rectangle {
                            id: removeAlertIcon
                            width: Math.max(52, metrics.fontPx(42))
                            height: width
                            anchors.verticalCenter: parent.verticalCenter
                            radius: metrics.radiusInner
                            color: themePalette.button
                            antialiasing: true

                            Text {
                                anchors.centerIn: parent
                                text: "!"
                                color: themePalette.buttonText
                                font.family: metrics.systemFont.family
                                font.pixelSize: metrics.fontPx(28)
                                font.bold: true
                            }
                        }

                        Column {
                            id: removeHeadingColumn
                            width: Math.max(0, parent.width - removeAlertIcon.width
                                                 - parent.spacing)
                            spacing: metrics.spaceCompact

                            Text {
                                width: parent.width
                                text: "Uygulamayı kaldır"
                                color: themePalette.text
                                font.family: metrics.systemFont.family
                                font.pixelSize: metrics.fontPageTitle
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                width: parent.width
                                text: page.appName
                                color: page.secondaryText
                                font.family: metrics.systemFont.family
                                font.pixelSize: metrics.fontBody
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: page.appName
                              + " uygulamasını sistemden kaldırmak istediğine emin misin?"
                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBodyLarge
                        wrapMode: Text.WordWrap
                        lineHeight: 1.18
                    }

                    Rectangle {
                        width: parent.width
                        height: Math.max(74, removeWarningText.implicitHeight
                                            + metrics.spaceMedium * 2)
                        radius: metrics.radiusInner
                        color: themePalette.base
                        border.color: page.secondaryText
                        border.width: 1
                        antialiasing: true

                        Text {
                            id: removeWarningText
                            x: metrics.spaceMedium
                            y: metrics.spaceMedium
                            width: Math.max(0, parent.width - metrics.spaceMedium * 2)
                            text: "Bu işlem yalnızca seçili paketi kaldırır. İşlem sırasında sistem yönetici yetkisi isteyebilir."
                            color: themePalette.highlight
                            font.family: metrics.systemFont.family
                            font.pixelSize: metrics.fontSmall
                            wrapMode: Text.WordWrap
                            lineHeight: 1.2
                        }
                    }

                    Row {
                        width: parent.width
                        height: Math.max(42, metrics.fontPx(42))
                        spacing: metrics.spaceNormal

                        Button {
                            id: cancelRemoveButton
                            text: "Vazgeç"
                            activeFocusOnTab: true
                            KeyNavigation.tab: confirmRemoveButton
                            KeyNavigation.backtab: confirmRemoveButton
                            width: Math.max(0, (parent.width - parent.spacing) / 2)
                            height: parent.height

                            background: Rectangle {
                                radius: metrics.radiusInner
                                color: parent.hovered ? themePalette.alternateBase : themePalette.button
                                border.color: page.secondaryText
                                border.width: 1
                                antialiasing: true
                            }

                            contentItem: Text {
                                text: parent.text
                                color: themePalette.buttonText
                                font.family: metrics.systemFont.family
                                font.pixelSize: metrics.fontBody
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            onClicked: removeConfirmDialog.close()
                        }

                        Button {
                            id: confirmRemoveButton
                            text: "Kaldır"
                            activeFocusOnTab: true
                            KeyNavigation.tab: cancelRemoveButton
                            KeyNavigation.backtab: cancelRemoveButton
                            width: Math.max(0, (parent.width - parent.spacing) / 2)
                            height: parent.height

                            background: Rectangle {
                                radius: metrics.radiusInner
                                color: themePalette.highlight
                                border.color: confirmRemoveButton.activeFocus
                                              ? themePalette.highlightedText : themePalette.highlight
                                border.width: confirmRemoveButton.activeFocus ? 3 : 1
                                antialiasing: true
                            }

                            contentItem: Text {
                                text: parent.text
                                color: themePalette.highlightedText
                                font.family: metrics.systemFont.family
                                font.pixelSize: metrics.fontBody
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            onClicked: {
                                removeConfirmDialog.close()
                                page.logsExpanded = false
                                page.lastActionMessage = ""
                                page.startRemoveTransaction()
                            }
                        }
                    }
                }
            }
        }
    }
}
