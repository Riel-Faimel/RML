import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 下载页右侧：选择加载器版本。点条目即选中并收起。
Rectangle {
    id: root

    property bool opened: false
    property string loaderName: ""
    property string mcVersion: ""
    property string selectedVersion: ""

    signal chosen(string mcVersion, string version)
    signal closed()

    visible: opened
    color: Theme.surface
    radius: Theme.radiusLarge
    border.width: 1
    border.color: Theme.border
    clip: true

    function openFor(loader, mc, current) {
        root.loaderName = loader
        root.mcVersion = mc
        root.selectedVersion = current
        root.opened = true
    }

    function close() {
        if (!root.opened) return;
        root.opened = false
        root.closed()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: qsTr("选择 %1 版本").arg(root.loaderName)
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
            }

            SoftButton {
                text: qsTr("关闭")
                onClicked: root.close()
            }
        }

        Text {
            Layout.fillWidth: true
            text: bridge.loaderVersionsBusy
                  ? qsTr("正在获取 %1 %2 的版本…").arg(root.loaderName).arg(root.mcVersion)
                  : bridge.loaderVersionsError
            color: bridge.loaderVersionsError.length > 0 ? Theme.danger : Theme.textSecondary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            visible: bridge.loaderVersionsBusy || bridge.loaderVersionsError.length > 0
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: bridge.loaderVersionOptions

            delegate: Rectangle {
                width: ListView.view.width
                height: 38
                radius: Theme.radiusSmall
                color: modelData === root.selectedVersion ? Theme.primarySoft : Theme.glassFill
                border.width: 1
                border.color: modelData === root.selectedVersion ? Theme.primary : Theme.glassBorder

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    verticalAlignment: Text.AlignVCenter
                    text: modelData
                    color: modelData === root.selectedVersion ? Theme.primary : Theme.text
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.selectedVersion = modelData
                        root.chosen(root.mcVersion, modelData)
                        root.opened = false
                    }
                }
            }
        }
    }
}
