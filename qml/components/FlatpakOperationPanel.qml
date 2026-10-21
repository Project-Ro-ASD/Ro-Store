import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Rectangle {
    id: panel
    property var manager: null
    property bool expanded: true
    readonly property bool hasActivity: manager && (manager.busy || manager.verifying || manager.operationStatus.length > 0)
    visible: hasActivity
    implicitHeight: contentColumn.implicitHeight + 24
    radius: metrics.radiusPanel
    color: palette.base
    border.color: palette.mid

    ThemeMetrics { id: metrics }
    SystemPalette { id: palette }

    ColumnLayout {
        id: contentColumn
        x: 12
        y: 12
        width: Math.max(0, parent.width - 24)
        spacing: metrics.spaceNormal

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: manager ? ("Flatpak işlemi · " + manager.operationStatus) : ""
                color: palette.text
                font.bold: true
                font.pixelSize: metrics.fontTitle
                wrapMode: Text.WordWrap
            }
            ThemedActionButton {
                text: panel.expanded ? "Logları Gizle" : "Logları Göster"
                Layout.preferredHeight: Math.max(40, metrics.fontPx(40))
                onClicked: panel.expanded = !panel.expanded
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: palette.windowText
            text: manager ? manager.operationName + "\n" + manager.operationStage : ""
        }
        ProgressBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            from: 0
            to: 100
            value: manager ? manager.operationProgress : 0
            indeterminate: manager && (manager.busy || manager.verifying) && !manager.progressAvailable
        }
        Label {
            Layout.fillWidth: true
            color: palette.windowText
            text: manager && manager.progressAvailable
                ? "%" + manager.operationProgress
                : manager && (manager.busy || manager.verifying)
                  ? "İşlem sürüyor · kesin yüzde henüz bilinmiyor"
                  : "Son işlem durumu"
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            visible: panel.expanded
            clip: true
            TextArea {
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                font.family: "monospace"
                color: palette.text
                text: manager ? manager.operationLog : ""
            }
        }
    }
}
