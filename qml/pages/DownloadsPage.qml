import QtQuick
import QtQuick.Controls
import RoStore 1.0

Item {
    id: page

    property var packageTransactionManager: null

    property bool narrow: width < 760
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

    // ÜST BAR
    Item {
        id: topBar

        x: page.sideMargin
        y: 20
        width: page.contentWidth
        height: 54

        Button {
            text: "← Geri"
            width: 95
            height: 36

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            onClicked: page.backRequested()
        }

        Column {
            anchors.left: parent.left
            anchors.leftMargin: 115
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: "Yüklemeler"
                color: "#f4f7fb"
                font.pixelSize: 22
                font.bold: true
            }

            Text {
                text: page.packageTransactionManager
                      ? page.packageTransactionManager.queuedCount + " işlem sırada"
                      : "Paket yöneticisi hazır değil"

                color: "#9aa4b2"
                font.pixelSize: 12
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
            spacing: 18

            // AKTİF İŞLEM BAŞLIĞI
            Text {
                x: page.sideMargin
                width: page.contentWidth

                text: "Aktif işlem"
                color: "#ffffff"
                font.pixelSize: 19
                font.bold: true
            }

            // AKTİF İŞLEM YOK
            Rectangle {
                visible: !page.packageTransactionManager
                         || page.packageTransactionManager.activeTransaction === null

                x: page.sideMargin
                width: page.contentWidth
                height: visible ? 100 : 0

                radius: 18
                color: "#16202b"
                border.color: "#263445"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Şu anda aktif paket işlemi yok."
                    color: "#9aa4b2"
                    font.pixelSize: 14
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

                radius: 18
                color: "#16202b"
                border.color: "#334155"
                border.width: 1
                clip: true

                Text {
                    id: activePackageName

                    x: 20
                    y: 18
                    width: parent.width - 40

                    text: activeCard.tx ? activeCard.tx.packageName : ""
                    color: "#ffffff"
                    font.pixelSize: 18
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

                    color: "#9aa4b2"
                    font.pixelSize: 13
                }

                Rectangle {
                    id: progressTrack

                    x: 20
                    y: 78
                    width: parent.width - 40
                    height: 26

                    radius: 9
                    color: "#0b1117"
                    border.color: "#334155"
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
                        radius: 9
                        color: "#2563eb"

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

                        color: "#dbeafe"
                        font.pixelSize: 12
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
                    }

                    color: "#b5c0cc"
                    font.pixelSize: 13
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

                    color: "#f87171"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Button {
                    visible: activeCard.tx !== null

                    text: "İptal"

                    width: 110
                    height: 36

                    anchors.right: parent.right
                    anchors.rightMargin: 20

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
                    color: "#ffffff"
                    font.pixelSize: 19
                    font.bold: true
                }

                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter

                    text: page.packageTransactionManager
                          ? page.packageTransactionManager.queuedCount + " işlem"
                          : "0 işlem"

                    color: "#9aa4b2"
                    font.pixelSize: 13
                }
            }

            Rectangle {
                visible: !page.packageTransactionManager
                         || page.packageTransactionManager.queuedCount === 0

                x: page.sideMargin
                width: page.contentWidth
                height: visible ? 82 : 0

                radius: 16
                color: "#121a22"
                border.color: "#243447"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Kuyrukta bekleyen işlem yok."
                    color: "#7f8b99"
                    font.pixelSize: 13
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

                    radius: 16
                    color: "#121a22"
                    border.color: "#243447"
                    border.width: 1

                    Text {
                        x: 18
                        y: 16
                        width: parent.width - 36

                        text: parent.tx ? parent.tx.packageName : ""
                        color: "#e5edf5"
                        font.pixelSize: 15
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

                        color: "#9aa4b2"
                        font.pixelSize: 13
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
