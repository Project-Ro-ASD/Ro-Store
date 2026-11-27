pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import RoStore 1.0
import "../components"

Item {
    id: page
    property var catalog
    property var flatpak
    property string initialQuery: ""
    property var roResults: []
    property var flatpakResults: []
    property bool pendingFlatpakCatalog: true
    signal backRequested()
    signal roSelected(var app)
    signal rpmSelected(var app)
    signal flatpakSelected(var app)

    ThemeMetrics { id: metrics }
    SystemPalette { id: palette }
    FedoraPackageModel { id: rpm }

    function searchAll() {
        const query = searchField.text.trim()
        roResults = query.length >= 2 && catalog ? catalog.searchSnapshot(query) : []
        updateFlatpakResults()
        if (query.length >= 2)
            rpm.search(query)
    }

    function updateFlatpakResults() {
        const query = searchField.text.trim().toLocaleLowerCase()
        const rows = flatpak ? flatpak.catalog : []
        const found = []
        if (query.length >= 2) {
            for (let i = 0; i < rows.length && found.length < 80; ++i) {
                const app = rows[i]
                if (app.scope !== "user" || app.origin !== "flathub") continue
                if (((app.name || "") + " " + (app.id || "") + " " +
                     (app.summary || "")).toLocaleLowerCase().indexOf(query) >= 0)
                    found.push(app)
            }
        }
        flatpakResults = found
    }

    function alternativeFor(appId, source) {
        if (!appId) return ""
        if (source !== "Fedora RPM") {
            const available = rpm.availableForAppId(appId)
            if (available.packageName) return "Fedora RPM olarak da mevcut"
        }
        if (source !== "Flathub") {
            for (let i = 0; i < flatpakResults.length; ++i)
                if (flatpakResults[i].id === appId)
                    return "Flathub üzerinden de mevcut"
        }
        return ""
    }

    Connections {
        target: page.flatpak
        function onInventoryChanged() {
            page.updateFlatpakResults()
            if (!page.pendingFlatpakCatalog || page.flatpak.loading) return
            const sources = page.flatpak.sources
            for (let i = 0; i < sources.length; ++i) {
                if (sources[i].scope === "user" && sources[i].name === "flathub") {
                    page.pendingFlatpakCatalog = false
                    page.flatpak.loadCatalog("user", "flathub")
                    return
                }
            }
        }
    }
    Connections {
        target: page.catalog
        function onModelReset() { page.searchAll() }
    }
    Component.onCompleted: {
        searchField.text = initialQuery
        if (flatpak && !flatpak.busy && !flatpak.loading)
            flatpak.refresh()
        searchAll()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Math.max(12, Math.min(36, page.width / 16))
        spacing: metrics.spaceMedium

        RowLayout {
            Layout.fillWidth: true
            ThemedIconButton {
                text: "Geri"
                themedIconName: "go-previous"
                onClicked: page.backRequested()
            }
            Label {
                text: "Tüm kaynaklarda ara"
                Layout.fillWidth: true
                color: palette.windowText
                font.family: metrics.systemFont.family
                font.pixelSize: metrics.fontPageTitle
                font.bold: true
            }
        }
        Label {
            text: "Ro-ASD RPM, Fedora RPM ve kullanıcı Flathub uygulamalarını birlikte keşfet."
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: palette.windowText
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "En az iki karakter yazın"
                onAccepted: page.searchAll()
            }
            ThemedActionButton {
                text: "Ara"
                accented: true
                enabled: searchField.text.trim().length >= 2
                onClicked: page.searchAll()
            }
        }
        Label {
            Layout.fillWidth: true
            visible: searchField.text.trim().length < 2
            text: "Aramak için en az iki karakter yazın."
            color: palette.windowText
        }
        Label {
            Layout.fillWidth: true
            visible: rpm.loading || (page.flatpak && page.flatpak.loading)
            text: "Kaynaklar taranıyor…"
            color: palette.windowText
        }
        Label {
            Layout.fillWidth: true
            visible: rpm.error.length > 0 && !rpm.loading
            text: "Fedora RPM: " + rpm.error
            wrapMode: Text.WordWrap
            color: palette.windowText
        }
        Label {
            Layout.fillWidth: true
            visible: page.flatpak && !page.flatpak.loading
                     && !page.flatpak.hasRemote("user", "flathub")
            text: "Kullanıcı Flathub kaynağı etkin değil. Flatpak bölümünden ekleyebilirsiniz."
            wrapMode: Text.WordWrap
            color: palette.windowText
        }
        ScrollView {
            id: searchScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: Math.max(0, searchScroll.availableWidth - 16)
                spacing: metrics.spaceMedium

                Label {
                    text: "Ro-ASD RPM (" + page.roResults.length + ")"
                    font.bold: true
                    color: palette.windowText
                }
                Repeater {
                    model: page.roResults
                    delegate: ThemedActionButton {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(54, metrics.fontPx(54))
                        wrapButtonText: true
                        text: modelData.name + "  ·  " + (modelData.version || "Sürüm belirsiz") +
                              "  ·  Ro-ASD RPM" +
                              (page.alternativeFor(modelData.id, "Ro-ASD RPM") ?
                               "\n" + page.alternativeFor(modelData.id, "Ro-ASD RPM") : "")
                        onClicked: page.roSelected(modelData)
                    }
                }
                Label {
                    text: "Fedora RPM (" + rpm.totalResults + ")"
                    font.bold: true
                    color: palette.windowText
                }
                Repeater {
                    model: rpm
                    delegate: ThemedActionButton {
                        required property int index
                        readonly property var appData: rpm.at(index)
                        Layout.fillWidth: true
                        visible: searchField.text.trim().length >= 2
                        Layout.preferredHeight: visible ? Math.max(54, metrics.fontPx(54)) : 0
                        wrapButtonText: true
                        text: (appData.displayName || appData.name || "") + "  ·  " +
                              (appData.version || "Sürüm belirsiz") + "  ·  Fedora RPM" +
                              (page.alternativeFor(appData.appId, "Fedora RPM") ?
                               "\n" + page.alternativeFor(appData.appId, "Fedora RPM") : "")
                        onClicked: page.rpmSelected(appData)
                    }
                }
                Label {
                    text: "Flathub (" + page.flatpakResults.length + ")"
                    font.bold: true
                    color: palette.windowText
                }
                Repeater {
                    model: page.flatpakResults
                    delegate: ThemedActionButton {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(54, metrics.fontPx(54))
                        wrapButtonText: true
                        text: (modelData.name || modelData.id) + "  ·  " +
                              (modelData.version || "Sürüm belirsiz") + "  ·  Flathub" +
                              (page.alternativeFor(modelData.id, "Flathub") ?
                               "\n" + page.alternativeFor(modelData.id, "Flathub") : "")
                        onClicked: page.flatpakSelected(modelData)
                    }
                }
            }
        }
    }
}
