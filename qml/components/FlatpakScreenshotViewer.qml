import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

// Full-size viewer for display-only Flathub screenshots.
// Kept isolated from package actions and Flatpak transaction state.
Popup {
    id: viewer

    property var screenshotUrls: []
    property string applicationName: ""
    property int currentIndex: 0
    property real zoomFactor: 1.0

    readonly property int imageCount: screenshotUrls ? screenshotUrls.length : 0
    readonly property string currentImage: imageCount > 0 && currentIndex >= 0
                                           && currentIndex < imageCount
                                           ? screenshotUrls[currentIndex] : ""

    modal: true
    focus: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.max(260, Math.min(1200, parent ? parent.width - 24 : 900))
    height: Math.max(260, Math.min(850, parent ? parent.height - 24 : 650))
    padding: Math.max(10, metrics.spaceNormal)
    closePolicy: Popup.CloseOnEscape

    ThemeMetrics { id: metrics }
    SystemPalette { id: palette }

    background: Rectangle {
        color: palette.window
        border.color: palette.mid
        border.width: 1
        radius: metrics.radiusPanel
    }

    function openAt(index) {
        if (imageCount <= 0 || index < 0 || index >= imageCount)
            return
        currentIndex = index
        zoomFactor = 1.0
        open()
    }

    function resetPosition() {
        imageViewport.contentX = Math.max(0, (imageViewport.contentWidth - imageViewport.width) / 2)
        imageViewport.contentY = Math.max(0, (imageViewport.contentHeight - imageViewport.height) / 2)
    }

    function changeZoom(value, anchorX, anchorY) {
        const nextZoom = Math.max(1.0, Math.min(4.0, Math.round(value * 100) / 100))
        if (nextZoom === zoomFactor)
            return

        // Keep the point under the cursor in the same place while zooming.
        // Toolbar and keyboard zoom stay anchored to the viewport center.
        const focusX = anchorX === undefined ? imageViewport.width / 2 : anchorX
        const focusY = anchorY === undefined ? imageViewport.height / 2 : anchorY
        const oldWidth = Math.max(1, imageViewport.contentWidth)
        const oldHeight = Math.max(1, imageViewport.contentHeight)
        const relativeX = (imageViewport.contentX + focusX) / oldWidth
        const relativeY = (imageViewport.contentY + focusY) / oldHeight
        zoomFactor = nextZoom
        imageViewport.contentX = Math.max(0, Math.min(
            imageViewport.contentWidth - imageViewport.width,
            relativeX * imageViewport.contentWidth - focusX))
        imageViewport.contentY = Math.max(0, Math.min(
            imageViewport.contentHeight - imageViewport.height,
            relativeY * imageViewport.contentHeight - focusY))
    }

    function changeImage(offset) {
        if (imageCount < 2)
            return
        currentIndex = (currentIndex + offset + imageCount) % imageCount
        zoomFactor = 1.0
        Qt.callLater(resetPosition)
    }

    onOpened: Qt.callLater(resetPosition)

    Shortcut {
        sequence: "Left"
        context: Qt.WindowShortcut
        enabled: viewer.opened && viewer.imageCount > 1
        onActivated: viewer.changeImage(-1)
    }
    Shortcut {
        sequence: "Right"
        context: Qt.WindowShortcut
        enabled: viewer.opened && viewer.imageCount > 1
        onActivated: viewer.changeImage(1)
    }
    Shortcut {
        sequence: "+"
        context: Qt.WindowShortcut
        enabled: viewer.opened
        onActivated: viewer.changeZoom(viewer.zoomFactor + 0.25)
    }
    Shortcut {
        sequence: "-"
        context: Qt.WindowShortcut
        enabled: viewer.opened
        onActivated: viewer.changeZoom(viewer.zoomFactor - 0.25)
    }
    Shortcut {
        sequence: "0"
        context: Qt.WindowShortcut
        enabled: viewer.opened
        onActivated: viewer.changeZoom(1.0)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: metrics.spaceNormal

        RowLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceNormal
            Label {
                Layout.fillWidth: true
                text: viewer.applicationName || "Ekran görüntüsü"
                font.pixelSize: metrics.fontTitle
                font.bold: true
                elide: Text.ElideRight
                color: palette.windowText
            }
            Label {
                text: (viewer.currentIndex + 1) + " / " + viewer.imageCount
                color: palette.windowText
                font.pixelSize: metrics.fontSmall
            }
            ThemedActionButton {
                text: "Kapat  ×"
                Layout.preferredWidth: Math.max(100, metrics.fontPx(100))
                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                onClicked: viewer.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: metrics.radiusInner
            color: palette.base
            border.color: palette.mid
            clip: true

            // Consume wheel events over the artwork, rather than allowing the
            // inner Flickable to scroll. Mouse wheel and touchpad both zoom.
            WheelHandler {
                id: artworkWheel
                target: null
                orientation: Qt.Vertical
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                acceptedModifiers: Qt.KeyboardModifierMask
                blocking: true
                enabled: viewer.opened && viewer.imageCount > 0
                onWheel: function(event) {
                    const angle = event.angleDelta.y
                    const pixels = event.pixelDelta.y
                    const steps = angle !== 0 ? angle / 120
                                : (pixels !== 0 ? pixels / 80 : 0)
                    if (steps === 0)
                        return
                    const focusX = Math.max(0, Math.min(imageViewport.width,
                        artworkWheel.point.position.x - imageViewport.x))
                    const focusY = Math.max(0, Math.min(imageViewport.height,
                        artworkWheel.point.position.y - imageViewport.y))
                    viewer.changeZoom(viewer.zoomFactor + steps * 0.25, focusX, focusY)
                    event.accepted = true
                }
            }

            Flickable {
                id: imageViewport
                anchors.fill: parent
                anchors.margins: 2
                clip: true
                contentWidth: Math.max(width, width * viewer.zoomFactor)
                contentHeight: Math.max(height, height * viewer.zoomFactor)
                boundsBehavior: Flickable.StopAtBounds
                interactive: viewer.zoomFactor > 1.0
                flickableDirection: Flickable.HorizontalAndVerticalFlick

                Image {
                    id: fullImage
                    x: 0
                    y: 0
                    width: imageViewport.contentWidth
                    height: imageViewport.contentHeight
                    source: viewer.currentImage
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: true
                    smooth: true
                }

                ScrollBar.horizontal: ScrollBar {
                    policy: viewer.zoomFactor > 1 ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }
                ScrollBar.vertical: ScrollBar {
                    policy: viewer.zoomFactor > 1 ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                visible: fullImage.status === Image.Loading
                running: visible
            }

            Label {
                anchors.centerIn: parent
                width: Math.max(0, parent.width - 24)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: palette.text
                visible: fullImage.status === Image.Error || fullImage.status === Image.Null
                text: fullImage.status === Image.Error
                    ? "Görsel yüklenemedi. Flathub görsel sunucusu erişilemez olabilir."
                    : "Görsel bulunamadı."
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: metrics.spaceCompact

            ThemedActionButton {
                text: "‹ Önceki"
                Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                enabled: viewer.imageCount > 1
                onClicked: viewer.changeImage(-1)
            }
            ThemedActionButton {
                text: "Sonraki ›"
                Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                enabled: viewer.imageCount > 1
                onClicked: viewer.changeImage(1)
            }
            Item { Layout.fillWidth: true }
            ThemedActionButton {
                text: "−"
                Layout.preferredWidth: 46
                Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                enabled: viewer.zoomFactor > 1
                onClicked: viewer.changeZoom(viewer.zoomFactor - 0.25)
            }
            Label {
                visible: viewer.width >= 760
                text: "Tekerlek: yakınlaştır / uzaklaştır"
                color: palette.windowText
                font.pixelSize: metrics.fontCaption
            }
            Label {
                text: Math.round(viewer.zoomFactor * 100) + "%"
                horizontalAlignment: Text.AlignHCenter
                Layout.preferredWidth: Math.max(48, metrics.fontPx(48))
                color: palette.windowText
            }
            ThemedActionButton {
                text: "+"
                Layout.preferredWidth: 46
                Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                enabled: viewer.zoomFactor < 4
                onClicked: viewer.changeZoom(viewer.zoomFactor + 0.25)
            }
            ThemedActionButton {
                text: "Sığdır"
                Layout.preferredWidth: Math.max(77, metrics.fontPx(77))
                Layout.preferredHeight: Math.max(42, metrics.fontPx(42))
                enabled: viewer.zoomFactor > 1
                onClicked: viewer.changeZoom(1.0)
            }
        }
    }
}
