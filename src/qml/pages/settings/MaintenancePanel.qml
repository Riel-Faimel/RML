import QtQuick
import QtQuick.Layouts
import "../.."
import "../../components"

// 设置页：核心与入口分离的维护入口（不做成完整管理页）。
GlassPanel {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40

            Text {
                text: qsTr("核心维护")
                font.pixelSize: 20
                font.bold: true
                color: Theme.text
                Layout.fillWidth: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        Text {
            Layout.fillWidth: true
            text: bridge.coreSummary
            font.pixelSize: 12
            color: Theme.textSecondary
            wrapMode: Text.WrapAnywhere
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 36
            clip: true
            contentHeight: reportText.height
            flickableDirection: Flickable.VerticalFlick

            Text {
                id: reportText
                width: parent.width
                text: bridge.maintenanceReport.length > 0
                          ? bridge.maintenanceReport
                          : qsTr("检查会列出待迁移的核心文件，不会改动任何数据。")
                font.pixelSize: 11
                color: Theme.textSecondary
                wrapMode: Text.WrapAnywhere
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            SoftButton {
                Layout.fillWidth: true
                text: qsTr("检查")
                textSize: 12
                implicitHeight: 30
                enabled: !bridge.busy
                onClicked: bridge.checkCores()
            }

            SoftButton {
                Layout.fillWidth: true
                text: qsTr("迁移")
                textSize: 12
                implicitHeight: 30
                primary: true
                enabled: !bridge.busy
                onClicked: bridge.migrateCores()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            SoftButton {
                Layout.fillWidth: true
                text: qsTr("回滚")
                textSize: 12
                implicitHeight: 30
                enabled: !bridge.busy
                onClicked: bridge.rollbackCoreMigration()
            }

            SoftButton {
                Layout.fillWidth: true
                text: qsTr("清理孤立核心")
                textSize: 12
                implicitHeight: 30
                enabled: !bridge.busy
                onClicked: bridge.cleanOrphanCores()
            }
        }
    }
}
