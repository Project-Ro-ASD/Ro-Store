import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import RoStore 1.0
import "../components"

Item {
    id: page
    property var flatpak: null
    property string source: "rpm"
    property bool applicationsOnly: true
    property var visibleRows: []
    property int visibleLimit: 48
    signal backRequested()
    signal flatpakRequested()

    SystemPalette { id: palette }
    ThemeMetrics { id: metrics }
    InstalledRpmModel { id: rpm }

    function formatSize(bytes) {
        const value = Number(bytes)
        if (!isFinite(value) || value < 0) return "Bilinmiyor"
        if (value < 1048576) return (value / 1024).toFixed(1) + " KB"
        if (value < 1073741824) return (value / 1048576).toFixed(1) + " MB"
        return (value / 1073741824).toFixed(2) + " GB"
    }

    function updateRows() {
        if (!flatpak) return
        const rows = source === "rpm" ? rpm.packages : flatpak.installed
        const query = search.text.trim().toLocaleLowerCase()
        const matching = []
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            if (source === "rpm" && applicationsOnly && rpm.applicationMetadataReady
                    && !row.isApplication)
                continue
            if (!query || ((row.displayName || "") + " " + (row.name || "")
                    + " " + (row.id || ""))
                    .toLocaleLowerCase().indexOf(query) >= 0)
                matching.push(row)
        }
        visibleRows = matching
    }

    function counterpart(row) {
        if (source === "rpm") {
            const ids = row.appIds || []
            const installed = flatpak.installed
            for (let i = 0; i < installed.length; ++i)
                if (ids.indexOf(installed[i].id) >= 0)
                    return "Flatpak: " + (installed[i].version || "Sürüm bilinmiyor")
                        + " · " + (installed[i].origin || "Kaynak bilinmiyor")
        } else {
            const installed = rpm.packages
            for (let i = 0; i < installed.length; ++i)
                if ((installed[i].appIds || []).indexOf(row.id) >= 0)
                    return "RPM: " + (installed[i].version || "Sürüm bilinmiyor")
        }
        return ""
    }

    Component.onCompleted: {
        rpm.refresh()
        flatpak.refresh()
    }
    onSourceChanged: {
        visibleLimit = 48
        updateRows()
    }
    onApplicationsOnlyChanged: {
        visibleLimit = 48
        updateRows()
    }

    Connections {
        target: rpm
        function onChanged() { page.updateRows() }
    }
    Connections {
        target: flatpak
        function onInventoryChanged() { page.updateRows() }
    }

    ScrollView {
        id: view
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.max(0, view.availableWidth - 28)
            x: 14
            y: 14
            spacing: metrics.spaceMedium

            RowLayout {
                Layout.fillWidth: true
                ThemedIconButton {
                    text: "Geri"
                    themedIconName: "go-previous"
                    Layout.preferredHeight: 42
                    onClicked: page.backRequested()
                }
                Label {
                    Layout.fillWidth: true
                    text: "Kurulu yazılımlar"
                    font.pixelSize: metrics.fontPageTitle
                    font.bold: true
                    color: palette.windowText
                    elide: Text.ElideRight
                }
                ThemedIconButton {
                    text: "Yenile"
                    themedIconName: "view-refresh"
                    Layout.preferredHeight: 42
                    enabled: !rpm.loading && !flatpak.loading && !flatpak.busy
                    onClicked: {
                        rpm.refresh()
                        flatpak.refresh()
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: "RPM ve Flatpak ayrı listelenir. Tüm RPM paketleri görünümü sistem bileşenlerini de içerir."
                wrapMode: Text.WordWrap
                color: palette.windowText
            }

            RowLayout {
                Layout.fillWidth: true
                visible: page.source === "rpm"
                Label {
                    text: "RPM görünümü"
                    color: palette.windowText
                }
                ComboBox {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    model: ["Uygulamalar", "Tüm paketler"]
                    currentIndex: page.applicationsOnly ? 0 : 1
                    onActivated: page.applicationsOnly = currentIndex === 0
                }
            }
            Label {
                Layout.fillWidth: true
                visible: page.source === "rpm" && !rpm.applicationMetadataReady
                text: "Uygulama kataloğu hazır değil; şimdilik tüm RPM paketleri gösteriliyor."
                wrapMode: Text.WordWrap
                color: palette.windowText
            }

            RowLayout {
                Layout.fillWidth: true
                ThemedActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    text: "RPM paketleri (" + rpm.packages.length + ")"
                    accented: page.source === "rpm"
                    onClicked: page.source = "rpm"
                }
                ThemedActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    text: "Flatpak uygulamaları (" + flatpak.installed.length + ")"
                    accented: page.source === "flatpak"
                    onClicked: page.source = "flatpak"
                }
            }

            TextField {
                id: search
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                placeholderText: "Kurulu yazılım ara..."
                onTextChanged: {
                    page.visibleLimit = 48
                    page.updateRows()
                }
            }

            Label {
                Layout.fillWidth: true
                visible: rpm.error.length > 0 && page.source === "rpm"
                text: rpm.error
                wrapMode: Text.WordWrap
                color: palette.windowText
            }
            Label {
                Layout.fillWidth: true
                visible: page.source === "flatpak" && !flatpak.available
                text: "Flatpak bu sistemde kurulu değil."
                color: palette.windowText
            }
            Label {
                Layout.fillWidth: true
                visible: (page.source === "rpm" ? rpm.loading : flatpak.loading)
                text: "Kurulu yazılımlar okunuyor..."
                color: palette.windowText
            }
            Label {
                Layout.fillWidth: true
                visible: !(page.source === "rpm" ? rpm.loading : flatpak.loading)
                         && page.visibleRows.length === 0
                text: "Eşleşen yazılım bulunamadı."
                color: palette.windowText
            }

            Repeater {
                model: page.visibleRows.slice(0, page.visibleLimit)
                delegate: Rectangle {
                    required property var modelData
                    readonly property string otherSource: page.counterpart(modelData)
                    Layout.fillWidth: true
                    implicitHeight: details.implicitHeight + 20
                    radius: metrics.radiusCard
                    color: palette.base
                    border.color: palette.mid
                    ColumnLayout {
                        id: details
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 12
                        y: 10
                        spacing: 4
                        Label {
                            Layout.fillWidth: true
                            text: modelData.displayName || modelData.name || modelData.id || "?"
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "Sürüm: " + (modelData.version || "Bilinmiyor")
                                  + " · Mimari: " + (modelData.architecture || "Bilinmiyor")
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: page.source === "rpm"
                                  ? "Kaynak: RPM · Kurulu boyut: " + page.formatSize(modelData.installedSize)
                                  : "Kaynak: " + (modelData.origin || "Bilinmiyor")
                                    + " (" + (modelData.scope === "user" ? "Kullanıcı" : "Sistem") + ")"
                                    + " · Kurulu boyut: " + (modelData.installedSize || "Bilinmiyor")
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: otherSource.length > 0
                            text: "Aynı uygulama diğer kaynakta da kurulu · " + otherSource
                            color: palette.windowText
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            ThemedActionButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                visible: page.visibleRows.length > page.visibleLimit
                text: "Daha fazla göster (" + Math.min(page.visibleLimit, page.visibleRows.length)
                      + " / " + page.visibleRows.length + ")"
                onClicked: page.visibleLimit += 48
            }
            ThemedActionButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                visible: page.source === "flatpak"
                text: "Flatpak yönetimine git"
                onClicked: page.flatpakRequested()
            }
            Item { Layout.fillWidth: true; Layout.preferredHeight: 14 }
        }
    }
}
