import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 下载页版本行：游戏版本 + 加载器 + 服务端 + 安装。
Rectangle {
    id: root

    property int index: -1
    property string versionId: ""
    property string loaderName: "原版"

    signal installRequested()
    signal serverRequested()
    signal loaderRequested(string loader)

    width: 0
    height: 46
    radius: Theme.radius
    color: Theme.glassFill
    border.width: 1
    border.color: Theme.glassBorder

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: root.versionId
            elide: Text.ElideRight
            font.pixelSize: 14
            color: Theme.text
        }

        Button {
            id: loaderButton
            text: root.loaderName
            enabled: !bridge.downloadBusy
            implicitWidth: 84
            implicitHeight: 30
            padding: 0
            onClicked: loaderMenu.popup()

            contentItem: Text {
                text: loaderButton.text
                color: Theme.primary
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                radius: Theme.radiusSmall
                color: loaderButton.hovered ? Theme.primarySoft : Theme.glassHoverFill
                border.width: 1
                border.color: Theme.primary
            }

            SoftMenu {
                id: loaderMenu
                y: loaderButton.height
                width: 160

                Repeater {
                    model: ["原版", "Fabric", "Forge", "NeoForge"]

                    delegate: SoftMenuItem {
                        text: modelData
                        onTriggered: root.loaderRequested(modelData)
                    }
                }
            }
        }

        SoftButton {
            text: qsTr("服务端")
            enabled: !bridge.downloadBusy
            implicitWidth: 68
            implicitHeight: 30
            textSize: 12
            onClicked: root.serverRequested()
        }

        Button {
            text: qsTr("安装")
            enabled: !bridge.downloadBusy
            implicitWidth: 76
            implicitHeight: 30
            contentItem: Text {
                text: parent.text
                color: Theme.primaryText
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: Theme.radiusSmall
                color: parent.enabled ? Theme.primary : Theme.disabledFill
            }
            onClicked: root.installRequested()
        }
    }
}
