import QtQuick
import QtQuick.Controls
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
    property var packageTransactionManager: null

    property bool narrow: width < Math.max(760, Math.ceil(760 * metrics.fontScale))
    property int sideMargin: narrow ? 22 : 36
    property int contentWidth: Math.max(320, width - sideMargin * 2)

    signal backRequested()

    function operationText(operation) {
        switch (operation) {
        case PackageTransaction.Install:
            return "Kurulum"
        case PackageTransaction.Remove:
            return "Kaldırma"
        case PackageTransaction.Upgrade:
            return "Güncelleme"
        }

        return "İşlem"
    }

    function stateText(state) {
        switch (state) {
        case PackageTransaction.Queued:
            return "Sırada"
        case PackageTransaction.Resolving:
            return "Hazırlanıyor"
        case PackageTransaction.Ready:
            return "Hazır"
        case PackageTransaction.Downloading:
            return "İndiriliyor"
        case PackageTransaction.Running:
            return "Uygulanıyor"
        case PackageTransaction.Finished:
            return "Tamamlandı"
        case PackageTransaction.Failed:
            return "Başarısız"
        case PackageTransaction.Cancelled:
            return "İptal edildi"
        }

        return "Bilinmiyor"
    }

    function formatBytes(bytes) {
        var value = Number(bytes)

        if (!isFinite(value) || value <= 0)
            return "0 B"

        if (value < 1024)
            return Math.round(value) + " B"

        value /= 1024

        if (value < 1024)
            return value.toFixed(1) + " KB"

        value /= 1024

        if (value < 1024)
            return value.toFixed(1) + " MB"

        value /= 1024
        return value.toFixed(2) + " GB"
    }

    function formatSpeed(bytesPerSecond) {
        var value = Number(bytesPerSecond)

        if (!isFinite(value) || value <= 0)
            return "Hesaplanıyor..."

        return formatBytes(value) + "/sn"
    }

    function formatEta(remainingBytes, bytesPerSecond) {
        var remaining = Number(remainingBytes)
        var speed = Number(bytesPerSecond)

        if (!isFinite(remaining)
                || !isFinite(speed)
                || remaining <= 0
                || speed <= 0)
            return ""

        var seconds = Math.ceil(remaining / speed)

        if (seconds <= 1)
            return "< 1 sn"

        if (seconds < 60)
            return seconds + " sn"

        var minutes = Math.floor(seconds / 60)
        var remainingSeconds = seconds % 60

        if (minutes < 60) {
            if (remainingSeconds === 0)
                return minutes + " dk"

            return minutes + " dk "
                   + remainingSeconds + " sn"
        }

        var hours = Math.floor(minutes / 60)
        var remainingMinutes = minutes % 60

        if (remainingMinutes === 0)
            return hours + " sa"

        return hours + " sa "
               + remainingMinutes + " dk"
    }

    // ÜST BAR
    Item {
        id: topBar

        x: page.sideMargin
        y: 20
        width: page.contentWidth
        height: Math.max(54, downloadsTitleColumn.implicitHeight + metrics.spaceNormal * 2)

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

            onClicked: page.backRequested()
        }

        Column {
            id: downloadsTitleColumn
            anchors.left: parent.left
            anchors.leftMargin: backButton.width + metrics.spaceNormal
            anchors.verticalCenter: parent.verticalCenter
            spacing: metrics.spaceCompact

            Text {
                text: "Yüklemeler"
                color: themePalette.windowText
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontPageTitle
                font.bold: true
            }

            Text {
                text: page.packageTransactionManager
                      ? page.packageTransactionManager.queuedCount + " işlem sırada"
                      : "Paket yöneticisi hazır değil"

                color: page.secondaryText
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontCaption
            }
        }
    }

    Flickable {
        id: flick

        x: 0
        y: topBar.y + topBar.height + 12

        width: parent.width
        height: parent.height - y

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
            spacing: metrics.spaceExtraLarge

            // AKTİF İŞLEM BAŞLIĞI
            Text {
                x: page.sideMargin
                width: page.contentWidth

                text: "Aktif işlem"
                color: themePalette.windowText
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontSection
                font.bold: true
            }

            // AKTİF İŞLEM YOK
            Rectangle {
                visible: !page.packageTransactionManager
                         || page.packageTransactionManager.activeTransaction === null

                x: page.sideMargin
                width: page.contentWidth
                height: visible ? 100 : 0

                radius: metrics.radiusPanel
                color: themePalette.base
                border.color: page.secondaryText
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Şu anda aktif paket işlemi yok."
                    color: page.secondaryText
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontBody
                }
            }

            // AKTİF İŞLEM
            Rectangle {
                id: activeCard

                property var tx: page.packageTransactionManager
                                 ? page.packageTransactionManager.activeTransaction
                                 : null

                visible: tx !== null

                x: page.sideMargin
                width: page.contentWidth
                height: visible ? (page.narrow ? 230 : 180) : 0

                radius: metrics.radiusPanel
                color: themePalette.base
                border.color: page.secondaryText
                border.width: 1
                clip: true

                Text {
                    id: activePackageName

                    x: 20
                    y: 18
                    width: parent.width - 40

                    text: activeCard.tx ? activeCard.tx.packageName : ""
                    color: themePalette.text
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontTitle
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    x: 20
                    y: 48
                    width: parent.width - 40

                    text: activeCard.tx
                          ? page.operationText(activeCard.tx.operation)
                            + " • "
                            + page.stateText(activeCard.tx.state)
                          : ""

                    color: page.secondaryText
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSmall
                }

                Rectangle {
                    id: progressTrack

                    x: 20
                    y: 78
                    width: parent.width - 40
                    height: 26

                    radius: metrics.radiusBadge
                    color: themePalette.button
                    border.color: themePalette.mid
                    border.width: 1
                    clip: true

                    Rectangle {
                        width: activeCard.tx
                               ? Math.max(
                                     0,
                                     Math.min(
                                         parent.width,
                                         parent.width
                                         * activeCard.tx.progress
                                         / 100
                                     )
                                 )
                               : 0

                        height: parent.height
                        radius: metrics.radiusBadge
                        color: themePalette.highlight

                        Behavior on width {
                            NumberAnimation {
                                duration: 120
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent

                        text: activeCard.tx
                              ? "%" + activeCard.tx.progress
                              : "%0"

                        color: activeCard.tx && activeCard.tx.progress >= 65
                               ? themePalette.highlightedText
                               : themePalette.buttonText
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontCaption
                        font.bold: true
                    }
                }

                Text {
                    x: 20
                    y: 118
                    width: page.narrow ? parent.width - 40 : parent.width - 190

                    text: {
                        if (!activeCard.tx)
                            return ""

                        if (activeCard.tx.totalBytes <= 0)
                            return "İndirme boyutu bekleniyor veya indirme gerekmiyor."

                        var remaining = Math.max(
                            0,
                            Number(activeCard.tx.totalBytes)
                            - Number(activeCard.tx.downloadedBytes)
                        )

                        return "İndirilen: "
                               + page.formatBytes(activeCard.tx.downloadedBytes)
                               + " / "
                               + page.formatBytes(activeCard.tx.totalBytes)
                               + "   •   Kalan: "
                               + page.formatBytes(remaining)
                               + (activeCard.tx.state === PackageTransaction.Downloading
                                  ? "   •   Hız: "
                                    + page.formatSpeed(
                                        activeCard.tx.downloadSpeedBytesPerSecond
                                      )
                                  : "")
                               + (activeCard.tx.state === PackageTransaction.Downloading
                                  && activeCard.tx.downloadSpeedBytesPerSecond > 0
                                  && remaining > 0
                                  ? "   •   Tahmini: "
                                    + page.formatEta(
                                        remaining,
                                        activeCard.tx.downloadSpeedBytesPerSecond
                                      )
                                  : "")
                    }

                    color: themePalette.text
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSmall
                    wrapMode: Text.WordWrap
                }

                Text {
                    visible: activeCard.tx
                             && activeCard.tx.errorMessage.length > 0

                    x: 20
                    y: page.narrow ? 155 : 145
                    width: parent.width - 40

                    text: activeCard.tx
                          ? activeCard.tx.errorMessage
                          : ""

                    color: themePalette.text
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontCaption
                    wrapMode: Text.WordWrap
                }

                Button {
                    visible: activeCard.tx !== null

                    text: "İptal"

                    width: 110
                    height: 36

                    anchors.right: parent.right
                    anchors.rightMargin: metrics.spaceExtraLarge

                    y: page.narrow ? 174 : 126

                    // DNF5 daemon yalnız paket indirme aşamasında
                    // güvenli iptale izin veriyor.
                    enabled: activeCard.tx
                             && activeCard.tx.state
                                === PackageTransaction.Downloading

                    onClicked: {
                        if (page.packageTransactionManager)
                            page.packageTransactionManager.cancelActive()
                    }
                }
            }

            // KUYRUK BAŞLIĞI
            Item {
                width: page.contentWidth
                height: 34
                x: page.sideMargin

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter

                    text: "Sıradaki işlemler"
                    color: themePalette.windowText
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSection
                    font.bold: true
                }

                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter

                    text: page.packageTransactionManager
                          ? page.packageTransactionManager.queuedCount + " işlem"
                          : "0 işlem"

                    color: page.secondaryText
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSmall
                }
            }

            Rectangle {
                visible: !page.packageTransactionManager
                         || page.packageTransactionManager.queuedCount === 0

                x: page.sideMargin
                width: page.contentWidth
                height: visible ? 82 : 0

                radius: metrics.radiusInner
                color: themePalette.base
                border.color: themePalette.button
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Kuyrukta bekleyen işlem yok."
                    color: page.secondaryText
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSmall
                }
            }

            Repeater {
                model: page.packageTransactionManager
                       ? page.packageTransactionManager.model
                       : null

                delegate: Rectangle {
                    property var tx: page.packageTransactionManager
                                     ? page.packageTransactionManager.model.get(index)
                                     : null

                    visible: tx !== null
                             && tx.state === PackageTransaction.Queued

                    x: page.sideMargin
                    width: page.contentWidth
                    height: visible ? 88 : 0

                    radius: metrics.radiusInner
                    color: themePalette.base
                    border.color: themePalette.button
                    border.width: 1

                    Text {
                        x: 18
                        y: 16
                        width: parent.width - 36

                        text: parent.tx ? parent.tx.packageName : ""
                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBodyLarge
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        x: 18
                        y: 46
                        width: parent.width - 36

                        text: parent.tx
                              ? page.operationText(parent.tx.operation)
                                + " • Sırada"
                              : ""

                        color: page.secondaryText
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontSmall
                    }
                }
            }

            // İŞLEM GEÇMİŞİ
            Item {
                width: page.contentWidth
                height: 34
                x: page.sideMargin

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter

                    text: "İşlem geçmişi"
                    color: themePalette.windowText
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontSection
                    font.bold: true
                }

                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter

                    text: "Bu oturum"
                    color: page.secondaryText
                    font.family: metrics.systemFont.family
                    font.pixelSize: metrics.fontCaption
                }
            }

            Repeater {
                model: page.packageTransactionManager
                       ? page.packageTransactionManager.model
                       : null

                delegate: Rectangle {
                    property var tx: page.packageTransactionManager
                                     ? page.packageTransactionManager.model.get(index)
                                     : null

                    readonly property bool terminalState:
                        tx !== null
                        && (
                            tx.state === PackageTransaction.Finished
                            || tx.state === PackageTransaction.Failed
                            || tx.state === PackageTransaction.Cancelled
                        )

                    visible: terminalState

                    x: page.sideMargin
                    width: page.contentWidth

                    height: visible
                            ? (
                                tx
                                && tx.errorMessage.length > 0
                                ? 112
                                : 88
                              )
                            : 0

                    radius: metrics.radiusInner
                    color: themePalette.base

                    border.color: tx
                                  && tx.state === PackageTransaction.Failed
                                  ? themePalette.button
                                  : tx
                                    && tx.state === PackageTransaction.Cancelled
                                    ? themePalette.button
                                    : themePalette.button

                    border.width: 1

                    Text {
                        x: 18
                        y: 14
                        width: parent.width - 150

                        text: parent.tx
                              ? parent.tx.packageName
                              : ""

                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontBodyLarge
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: metrics.spaceExtraLarge
                        y: 15

                        text: {
                            if (!parent.tx)
                                return ""

                            if (parent.tx.state === PackageTransaction.Finished)
                                return "Tamamlandı"

                            if (parent.tx.state === PackageTransaction.Failed)
                                return "Başarısız"

                            if (parent.tx.state === PackageTransaction.Cancelled)
                                return "İptal edildi"

                            return ""
                        }

                        color: {
                            if (!parent.tx)
                                return page.secondaryText

                            if (parent.tx.state === PackageTransaction.Finished)
                                return page.mutedAccent

                            if (parent.tx.state === PackageTransaction.Failed)
                                return themePalette.text

                            if (parent.tx.state === PackageTransaction.Cancelled)
                                return page.secondaryText

                            return page.secondaryText
                        }

                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontCaption
                        font.bold: true
                    }

                    Text {
                        x: 18
                        y: 44
                        width: parent.width - 36

                        text: parent.tx
                              ? page.operationText(parent.tx.operation)
                                + " • "
                                + page.stateText(parent.tx.state)
                              : ""

                        color: page.secondaryText
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontSmall
                    }

                    Text {
                        visible: parent.tx
                                 && parent.tx.errorMessage.length > 0

                        x: 18
                        y: 72
                        width: parent.width - 36

                        text: parent.tx
                              ? parent.tx.errorMessage
                              : ""

                        color: themePalette.text
                        font.family: metrics.systemFont.family
                        font.pixelSize: metrics.fontCaption
                        elide: Text.ElideRight
                    }
                }
            }

            Item {
                width: 1
                height: 24
            }
        }
    }
}
