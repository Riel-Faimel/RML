import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap

        // 左：版本列表
        GlassPanel {
            backdrop: page.backdrop
            Layout.preferredWidth: 280
            Layout.fillHeight: true

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
                        color: modelData === bridge.selected ? Theme.primarySoft : "#66FFFFFF"
                        border.width: 1
                        border.color: modelData === bridge.selected ? Theme.primary : "#55FFFFFF"

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            verticalAlignment: Text.AlignVCenter
                            text: modelData
                            elide: Text.ElideRight
                            font.pixelSize: 14
                            color: modelData === bridge.selected ? Theme.primary : Theme.text
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: bridge.selectVersion(modelData)
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

        // 右：详情
        GlassPanel {
            backdrop: page.backdrop
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: bridge.selected
                        font.pixelSize: 24
                        font.bold: true
                        color: Theme.text
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        radius: 999
                        color: Theme.primary
                        implicitWidth: loaderText.implicitWidth + 22
                        implicitHeight: 26

                        Text {
                            id: loaderText
                            anchors.centerIn: parent
                            text: bridge.loader
                            font.pixelSize: 12
                            color: "#FFFFFF"
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: bridge.details
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    lineHeight: 1.4
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#33808080"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        Layout.fillWidth: true
                        text: bridge.javaInfo
                        font.pixelSize: 13
                        color: Theme.text
                        wrapMode: Text.WordWrap
                    }
                }

                Item { Layout.fillHeight: true }

                Text {
                    Layout.fillWidth: true
                    text: bridge.status
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("补齐文件")
                        enabled: !bridge.busy
                        implicitWidth: 120
                        implicitHeight: 46

                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? Theme.primary : Theme.textSecondary
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: Theme.radius
                            color: parent.hovered ? Theme.primarySoft : "#88FFFFFF"
                            border.width: 1
                            border.color: Theme.primary
                        }

                        onClicked: bridge.repair()
                    }

                    Button {
                        text: bridge.busy ? qsTr("运行中") : qsTr("启动")
                        enabled: !bridge.busy && bridge.selected.length > 0
                        implicitWidth: 160
                        implicitHeight: 46

                        contentItem: Text {
                            text: parent.text
                            color: "#FFFFFF"
                            font.pixelSize: 15
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: Theme.radius
                            color: !parent.enabled ? "#99FFFFFF" : (parent.pressed ? Theme.primaryPressed : Theme.primary)
                        }

                        onClicked: bridge.launchGame()
                    }
                }
            }
        }
    }
}
