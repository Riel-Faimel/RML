import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../.."
import "../../components"

// 设置页：外观（背景图）。
GlassPanel {
    id: root

    FileDialog {
        id: backgroundDialog
        title: qsTr("选择背景图片")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)")]
        onAccepted: bridge.setBackground(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Text {
            text: qsTr("外观")
            font.pixelSize: 20
            font.bold: true
            color: Theme.text
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("使用自己的图片作为启动器背景。")
            font.pixelSize: 12
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: bridge.hasCustomBackground ? qsTr("已自定义") : qsTr("默认背景")
                font.pixelSize: 12
                color: bridge.hasCustomBackground ? Theme.primary : Theme.textSecondary
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            SoftButton {
                text: qsTr("选择图片")
                primary: true
                onClicked: backgroundDialog.open()
            }

            SoftButton {
                text: qsTr("恢复默认")
                enabled: bridge.hasCustomBackground
                onClicked: bridge.resetBackground()
            }

            Item { Layout.fillWidth: true }
        }
    }
}
