import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 服务器页：实例列表。
GlassPanel {
    id: root

    signal addRequested()
    signal deleteRequested(int index, string name)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: qsTr("服务器")
                font.pixelSize: 20
                font.bold: true
                color: Theme.text
            }

            Button {
                id: addButton
                implicitWidth: 32
                implicitHeight: 32
                padding: 0
                onClicked: root.addRequested()

                contentItem: Text {
                    text: "+"
                    font.pixelSize: 20
                    color: Theme.surface
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 999
                    color: addButton.pressed ? Theme.primaryPressed : Theme.primary
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        ListView {
            id: serverList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: serverBridge.instances

            delegate: Rectangle {
                id: serverRow
                width: ListView.view.width
                height: 54
                radius: Theme.radius
                color: index === serverBridge.selectedIndex ? Theme.primarySoft : Theme.glassFill
                border.width: 1
                border.color: index === serverBridge.selectedIndex ? Theme.primary : Theme.glassBorder

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 40
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: modelData.name
                        elide: Text.ElideRight
                        font.pixelSize: 14
                        color: index === serverBridge.selectedIndex ? Theme.primary : Theme.text
                    }

                    Text {
                        Layout.maximumWidth: 96
                        text: modelData.status
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        color: modelData.running ? Theme.primary : Theme.textSecondary
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: serverBridge.selectAt(index)
                }

                Button {
                    id: rowMoreButton
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    height: 30
                    padding: 0
                    onClicked: rowMenu.popup()

                    contentItem: Text {
                        text: "⋯"
                        font.pixelSize: 18
                        color: Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: rowMoreButton.hovered ? Theme.glassHoverFill : "transparent"
                    }
                }

                SoftMenu {
                    id: rowMenu
                    y: rowMoreButton.height

                    SoftMenuItem {
                        text: qsTr("删除服务器")
                        onTriggered: root.deleteRequested(index, modelData.name)
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: serverList.count === 0
                text: qsTr("还没有服务器")
                color: Theme.textSecondary
            }
        }
    }
}
