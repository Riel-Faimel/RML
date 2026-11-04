import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 启动页：版本列表。
GlassPanel {
    id: root

    signal renameRequested(int index, string name)
    signal deleteRequested(int index, string name)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("版本")
                font.pixelSize: 13
                color: Theme.textSecondary
                Layout.fillWidth: true
            }
            Text {
                text: bridge.versions.length
                font.pixelSize: 13
                color: Theme.textSecondary
            }
        }

        ListView {
            id: versionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: bridge.versions

            delegate: Rectangle {
                id: entry
                width: ListView.view.width
                height: 46
                radius: Theme.radius
                color: index === bridge.selectedIndex ? Theme.primarySoft : Theme.glassFill
                border.width: 1
                border.color: index === bridge.selectedIndex ? Theme.primary : Theme.glassBorder

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 40
                    verticalAlignment: Text.AlignVCenter
                    text: modelData
                    elide: Text.ElideRight
                    font.pixelSize: 14
                    color: index === bridge.selectedIndex ? Theme.primary : Theme.text
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: bridge.selectVersionAt(index)
                }

                Button {
                    id: moreButton
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    height: 30
                    padding: 0
                    onClicked: entryMenu.popup()

                    contentItem: Text {
                        text: "⋯"
                        font.pixelSize: 18
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: moreButton.hovered ? Theme.glassHoverFill : "transparent"
                    }
                }

                SoftMenu {
                    id: entryMenu
                    y: moreButton.height

                    SoftMenuItem {
                        text: qsTr("打开项目文件夹")
                        onTriggered: bridge.openVersionFolderAt(index)
                    }
                    SoftMenuItem {
                        text: qsTr("重命名")
                        onTriggered: root.renameRequested(index, modelData)
                    }
                    SoftMenuItem {
                        text: qsTr("删除版本")
                        onTriggered: root.deleteRequested(index, modelData)
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: versionList.count === 0
                text: qsTr("没有找到版本")
                color: Theme.textSecondary
            }
        }
    }
}
