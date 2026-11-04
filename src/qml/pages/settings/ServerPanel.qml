import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 设置页：服务端安装管理（不是服务器实例）。
GlassPanel {
    id: root

    property Item backdrop: null

    Component.onCompleted: serverBridge.reloadInstallations()
    onVisibleChanged: {
        if (visible) serverBridge.reloadInstallations()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40

            Text {
                text: qsTr("服务端管理")
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

        ListView {
            id: installList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: serverBridge.installations

            delegate: Rectangle {
                width: ListView.view.width
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
                        Layout.minimumWidth: 0
                        text: modelData.label ? modelData.label : modelData.id
                        color: Theme.text
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    SoftButton {
                        text: qsTr("删除")
                        textSize: 11
                        implicitWidth: 56
                        implicitHeight: 28
                        onClicked: serverBridge.removeInstallationAt(index)
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: installList.count === 0
                text: qsTr("还没有服务端安装，先在下载页安装服务端。")
                color: Theme.textSecondary
                font.pixelSize: 12
            }
        }
    }
}
