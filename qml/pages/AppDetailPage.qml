import QtQuick
import QtQuick.Controls
import RoStore 1.0

Item {
    id: page

    property string appName: ""
    property string summaryText: ""
    property string descriptionText: ""
    property string categoryText: ""
    property string versionText: ""
    property string packageName: ""
    property string iconUrl: ""

    property bool narrow: width < 760
    property int sideMargin: narrow ? 22 : 36
    property int contentWidth: Math.max(320, width - sideMargin * 2)

    property bool logsExpanded: false
    property string lastActionMessage: ""
    property bool lastActionSuccess: true

    property string displayStatusText: packageInstaller.running
                                       ? packageInstaller.statusText
                                       : lastActionMessage.length > 0
                                           ? lastActionMessage
                                           : packageStatus.statusText

    property color displayStatusColor: packageInstaller.running
                                       ? "#fbbf24"
                                       : lastActionMessage.length > 0
                                           ? (lastActionSuccess ? "#6ee7b7" : "#f87171")
                                           : (packageStatus.installed ? "#6ee7b7" : "#fbbf24")

    signal backRequested()

    PackageStatus {
        id: packageStatus
    }

    AppLauncher {
        id: appLauncher
    }

    PackageInstaller {
        id: packageInstaller

        onFinished: function(success, message) {
            page.lastActionMessage = message
            page.lastActionSuccess = success
            page.logsExpanded = false
            packageStatus.checkInstalled(page.packageName, page.versionText)
        }
    }

    Component.onCompleted: {
        packageStatus.checkInstalled(page.packageName, page.versionText)
    }

    // ÜST BAR
    Rectangle {
        id: topBar

        x: page.sideMargin
        y: 20
        width: page.contentWidth
        height: 44
        color: "transparent"

        Button {
            id: backButton
            text: "← Geri"
            width: 95
            height: 36
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            enabled: !packageInstaller.running
            onClicked: page.backRequested()
        }

        Text {
            text: "Ro-Store"
            color: "#9aa4b2"
            font.pixelSize: 14
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
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
            spacing: 18

            // UYGULAMA BAŞLIK KARTI
            Rectangle {
                width: page.contentWidth
                height: page.narrow ? 300 : 180
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
                        x: 26
                        y: 42
                        width: 96
                        height: 96
                        radius: 24
                        color: "#243447"
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
                            color: "#ffffff"
                            font.pixelSize: 38
                            font.bold: true
                        }
                    }

                    Text {
                        x: 148
                        y: 34
                        width: parent.width - 180
                        text: page.appName
                        color: "#ffffff"
                        font.pixelSize: 32
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        x: 148
                        y: 82
                        width: parent.width - 180
                        text: page.summaryText
                        color: "#b5c0cc"
                        font.pixelSize: 16
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Row {
                        x: 148
                        y: 123
                        spacing: 10

                        Rectangle {
                            radius: 10
                            color: "#243447"
                            height: 32
                            width: categoryLabelWide.implicitWidth + 24

                            Text {
                                id: categoryLabelWide
                                anchors.centerIn: parent
                                text: page.categoryText
                                color: "#79b8ff"
                                font.pixelSize: 13
                            }
                        }

                        Rectangle {
                            radius: 10
                            color: "#243447"
                            height: 32
                            width: versionLabelWide.implicitWidth + 24

                            Text {
                                id: versionLabelWide
                                anchors.centerIn: parent
                                text: "Sürüm " + page.versionText
                                color: "#c7d0dd"
                                font.pixelSize: 13
                            }
                        }
                    }
                }

                // Küçük ekran düzeni
                Column {
                    visible: page.narrow
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12

                    Rectangle {
                        width: 92
                        height: 92
                        radius: 24
                        color: "#243447"
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
                            color: "#ffffff"
                            font.pixelSize: 36
                            font.bold: true
                        }
                    }

                    Text {
                        width: parent.width
                        text: page.appName
                        color: "#ffffff"
                        font.pixelSize: 28
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: page.summaryText
                        color: "#b5c0cc"
                        font.pixelSize: 15
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
                            width: categoryLabelNarrow.implicitWidth + 24

                            Text {
                                id: categoryLabelNarrow
                                anchors.centerIn: parent
                                text: page.categoryText
                                color: "#79b8ff"
                                font.pixelSize: 13
                            }
                        }

                        Rectangle {
                            radius: 10
                            color: "#243447"
                            height: 32
                            width: versionLabelNarrow.implicitWidth + 24

                            Text {
                                id: versionLabelNarrow
                                anchors.centerIn: parent
                                text: "Sürüm " + page.versionText
                                color: "#c7d0dd"
                                font.pixelSize: 13
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

                radius: 18
                color: "#171d24"
                border.color: "#2a3440"
                border.width: 1
                antialiasing: true

                Column {
                    id: infoColumn

                    x: 24
                    y: 24
                    width: parent.width - 48
                    spacing: 15

                    Text {
                        width: parent.width
                        text: "Açıklama"
                        color: "#ffffff"
                        font.pixelSize: 22
                        font.bold: true
                    }

                    Text {
                        width: parent.width
                        text: page.descriptionText
                        color: "#b8c2cc"
                        font.pixelSize: 15
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: "#2a3440"
                    }

                    Text {
                        width: parent.width
                        text: "Paket bilgileri"
                        color: "#ffffff"
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Text {
                        width: parent.width
                        text: "Paket adı: " + page.packageName
                        color: "#9aa4b2"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: "Kaynak: Ro-Repo"
                        color: "#9aa4b2"
                        font.pixelSize: 14
                    }

                    Text {
                        width: parent.width
                        text: "Kurulu sürüm: " + (packageStatus.installedVersion.length > 0 ? packageStatus.installedVersion : "-")
                        color: "#9aa4b2"
                        font.pixelSize: 14
                    }

                    Text {
                        width: parent.width
                        text: "Repo sürümü: " + (page.versionText.length > 0 ? page.versionText : "-")
                        color: "#9aa4b2"
                        font.pixelSize: 14
                    }

                    Text {
                        width: parent.width
                        text: page.displayStatusText
                        color: page.displayStatusColor
                        font.pixelSize: 14
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        text: packageStatus.installed
                              ? "Bu uygulama sistemde yüklü."
                              : "Kur/Güncelle/Kaldır işlemleri PackageKit üzerinden yapılır."
                        color: "#7f8b99"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // TEKNİK LOG AYRINTILARI - varsayılan kapalı
            Item {
                visible: packageInstaller.running || packageInstaller.output.length > 0

                width: page.contentWidth
                height: visible ? (page.logsExpanded ? (page.narrow ? 260 : 285) : 42) : 0
                x: page.sideMargin

                Behavior on height {
                    NumberAnimation {
                        duration: 150
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 42
                    radius: 12
                    color: logToggleMouse.containsMouse ? "#111827" : "transparent"
                    border.color: logToggleMouse.containsMouse ? "#243447" : "transparent"
                    border.width: 1
                    antialiasing: true

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8

                        Text {
                            text: page.logsExpanded ? "⌄" : "›"
                            color: "#64748b"
                            font.pixelSize: 18
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: page.logsExpanded
                                  ? "Teknik işlem günlüklerini gizle"
                                  : "Teknik işlem günlüklerini göster"
                            color: "#64748b"
                            font.pixelSize: 13
                            font.italic: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: packageInstaller.running ? "• işlem devam ediyor" : "• ayrıntılar hazır"
                            color: packageInstaller.running ? "#fbbf24" : "#475569"
                            font.pixelSize: 12
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
                    anchors.topMargin: 48
                    height: parent.height - 48
                    radius: 16
                    color: "#0b1117"
                    border.color: "#334155"
                    border.width: 1
                    antialiasing: true
                    clip: true

                    Text {
                        x: 16
                        y: 12
                        text: "Teknik İşlem Günlüğü"
                        color: "#dbeafe"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        y: 14
                        text: "pkcon / PackageKit çıktısı"
                        color: "#475569"
                        font.pixelSize: 12
                    }

                    Rectangle {
                        x: 16
                        y: 38
                        width: parent.width - 32
                        height: 1
                        color: "#1f2937"
                    }

                    ScrollView {
                        x: 12
                        y: 48
                        width: parent.width - 24
                        height: parent.height - 60
                        clip: true

                        TextArea {
                            text: packageInstaller.output.length > 0
                                  ? packageInstaller.output
                                  : "Henüz işlem çıktısı yok."

                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.Wrap
                            font.family: "monospace"
                            font.pixelSize: 13
                            color: "#dbeafe"

                            background: Rectangle {
                                color: "#0b1117"
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
        height: page.narrow ? (packageInstaller.running ? 132 : 104) : 62

        radius: 16
        color: "#101820"
        border.color: "#243447"
        border.width: 1
        antialiasing: true

        // Geniş ekran alt bar
        Item {
            visible: !page.narrow
            anchors.fill: parent

            Button {
                text: "Durumu Yenile"
                width: 130
                height: 38
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                enabled: !packageStatus.checking && !packageInstaller.running
                onClicked: packageStatus.checkInstalled(page.packageName, page.versionText)
            }

            Text {
                x: 155
                width: parent.width - (packageInstaller.running ? 665 : 455)
                anchors.verticalCenter: parent.verticalCenter
                text: page.displayStatusText
                color: page.displayStatusColor
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            Rectangle {
                visible: packageInstaller.running
                width: 185
                height: 30
                anchors.right: parent.right
                anchors.rightMargin: 282
                anchors.verticalCenter: parent.verticalCenter

                radius: 10
                color: "#0b1117"
                border.color: "#334155"
                border.width: 1
                clip: true
                antialiasing: true

                Rectangle {
                    width: Math.max(0, parent.width * packageInstaller.progress / 100)
                    height: parent.height
                    radius: 10
                    color: "#2563eb"
                    opacity: 0.85
                    antialiasing: true

                    Behavior on width {
                        NumberAnimation {
                            duration: 120
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: packageInstaller.phaseText + "  %" + packageInstaller.progress
                    color: "#dbeafe"
                    font.pixelSize: 12
                    font.bold: true
                }
            }

            Button {
                visible: packageStatus.installed && !packageInstaller.running
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

            Button {
                text: packageInstaller.running
                      ? "İşlem sürüyor..."
                      : !packageStatus.installed
                          ? "Kur"
                          : packageStatus.updateAvailable
                              ? "Güncelle"
                              : "Kaldır"

                width: 130
                height: 38
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter

                enabled: !packageStatus.checking && !packageInstaller.running

                onClicked: {
                    page.logsExpanded = false
                    page.lastActionMessage = ""

                    if (!packageStatus.installed) {
                        packageInstaller.installPackage(page.packageName)
                    } else if (packageStatus.updateAvailable) {
                        page.logsExpanded = false
                        page.lastActionMessage = ""
                        packageInstaller.updatePackage(page.packageName)
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
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            Rectangle {
                visible: packageInstaller.running
                x: 12
                y: 38
                width: parent.width - 24
                height: 28

                radius: 10
                color: "#0b1117"
                border.color: "#334155"
                border.width: 1
                clip: true
                antialiasing: true

                Rectangle {
                    width: Math.max(0, parent.width * packageInstaller.progress / 100)
                    height: parent.height
                    radius: 10
                    color: "#2563eb"
                    opacity: 0.85
                    antialiasing: true

                    Behavior on width {
                        NumberAnimation {
                            duration: 120
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: packageInstaller.phaseText + "  %" + packageInstaller.progress
                    color: "#dbeafe"
                    font.pixelSize: 12
                    font.bold: true
                }
            }

            Button {
                visible: packageStatus.installed && !packageInstaller.running
                text: "Çalıştır"
                x: 12
                y: 54
                width: (parent.width - 48) / 3
                height: 38

                onClicked: {
                    appLauncher.launch(page.packageName)
                    page.lastActionMessage = appLauncher.statusText
                    page.lastActionSuccess = true
                }
            }

            Button {
                text: "Durumu Yenile"
                x: packageStatus.installed && !packageInstaller.running ? 12 + ((parent.width - 48) / 3) + 12 : 12
                y: packageInstaller.running ? 82 : 54
                width: packageStatus.installed && !packageInstaller.running ? (parent.width - 48) / 3 : (parent.width - 36) / 2
                height: 38
                enabled: !packageStatus.checking && !packageInstaller.running
                onClicked: packageStatus.checkInstalled(page.packageName, page.versionText)
            }

            Button {
                text: packageInstaller.running
                      ? "İşlem..."
                      : !packageStatus.installed
                          ? "Kur"
                          : packageStatus.updateAvailable
                              ? "Güncelle"
                              : "Kaldır"

                x: packageStatus.installed && !packageInstaller.running ? 36 + 2 * ((parent.width - 48) / 3) : 24 + (parent.width - 36) / 2
                y: packageInstaller.running ? 82 : 54
                width: packageStatus.installed && !packageInstaller.running ? (parent.width - 48) / 3 : (parent.width - 36) / 2
                height: 38

                enabled: !packageStatus.checking && !packageInstaller.running

                onClicked: {
                    page.logsExpanded = false
                    page.lastActionMessage = ""

                    if (!packageStatus.installed) {
                        packageInstaller.installPackage(page.packageName)
                    } else if (packageStatus.updateAvailable) {
                        page.logsExpanded = false
                        page.lastActionMessage = ""
                        packageInstaller.updatePackage(page.packageName)
                    } else {
                        removeConfirmDialog.open()
                    }
                }
            }
        }
    }

    // KALDIRMA ONAY PENCERESİ
    Item {
        id: removeConfirmDialog

        anchors.fill: parent
        visible: false
        z: 9999

        function open() {
            visible = true
        }

        function close() {
            visible = false
        }

        Rectangle {
            anchors.fill: parent
            color: "#99000000"

            MouseArea {
                anchors.fill: parent
                onClicked: removeConfirmDialog.close()
            }
        }

        Rectangle {
            id: removePanel

            width: Math.min(520, page.width - 64)
            height: 360
            anchors.centerIn: parent

            radius: 22
            color: "#171d24"
            border.color: "#334155"
            border.width: 1
            clip: true
            antialiasing: true
            layer.enabled: true
            layer.smooth: true

            MouseArea {
                anchors.fill: parent
                onClicked: mouse.accepted = true
            }

            Rectangle {
                x: 1
                y: 1
                width: parent.width - 2
                height: 94
                radius: 21
                color: "#16202b"
                antialiasing: true
            }

            Rectangle {
                x: 1
                y: 58
                width: parent.width - 2
                height: 37
                color: "#16202b"
            }

            Rectangle {
                x: 22
                y: 21
                width: 52
                height: 52
                radius: 16
                color: "#3a1f1f"
                antialiasing: true

                Text {
                    anchors.centerIn: parent
                    text: "!"
                    color: "#fca5a5"
                    font.pixelSize: 28
                    font.bold: true
                }
            }

            Text {
                x: 90
                y: 22
                width: parent.width - 112
                text: "Uygulamayı kaldır"
                color: "#ffffff"
                font.pixelSize: 22
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                x: 90
                y: 54
                width: parent.width - 112
                text: page.appName
                color: "#9aa4b2"
                font.pixelSize: 14
                elide: Text.ElideRight
            }

            Text {
                x: 22
                y: 120
                width: parent.width - 44
                text: page.appName + " uygulamasını sistemden kaldırmak istediğine emin misin?"
                color: "#dbeafe"
                font.pixelSize: 15
                wrapMode: Text.WordWrap
                lineHeight: 1.18
            }

            Rectangle {
                x: 22
                y: 190
                width: parent.width - 44
                height: 74
                radius: 14
                color: "#0b1117"
                border.color: "#334155"
                border.width: 1
                antialiasing: true

                Text {
                    anchors.fill: parent
                    anchors.margins: 12
                    text: "Bu işlem yalnızca seçili paketi kaldırır. İşlem sırasında sistem yönetici yetkisi isteyebilir."
                    color: "#fbbf24"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    lineHeight: 1.2
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Row {
                width: 336
                height: 42
                spacing: 16
                x: (parent.width - width) / 2
                y: parent.height - 62

                Button {
                    text: "Vazgeç"
                    width: 160
                    height: 42

                    background: Rectangle {
                        radius: 16
                        color: parent.down ? "#d1d5db" : parent.hovered ? "#f3f4f6" : "#ffffff"
                        border.color: "#d1d5db"
                        border.width: 1
                        antialiasing: true
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "#111827"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: removeConfirmDialog.close()
                }

                Button {
                    text: "Kaldır"
                    width: 160
                    height: 42

                    background: Rectangle {
                        radius: 16
                        color: parent.down ? "#7f1d1d" : parent.hovered ? "#991b1b" : "#b91c1c"
                        border.color: "#ef4444"
                        border.width: 1
                        antialiasing: true
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        removeConfirmDialog.close()
                        page.logsExpanded = false
                        page.lastActionMessage = ""
                        packageInstaller.removePackage(page.packageName)
                    }
                }
            }
        }
    }
}
