import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null

    GlassPanel {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        backdrop: page.backdrop

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14

            Text {
                text: qsTr("Java 运行时")
                font.pixelSize: 20
                font.bold: true
                color: Theme.text
            }

            ListView {
                id: javaList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: bridge.javas

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 38
                    radius: Theme.radiusSmall
                    color: "#66FFFFFF"
                    border.width: 1
                    border.color: "#55FFFFFF"

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        verticalAlignment: Text.AlignVCenter
                        text: modelData
                        elide: Text.ElideMiddle
                        font.pixelSize: 13
                        color: Theme.text
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: javaList.count === 0
                    text: qsTr("没有发现 Java")
                    color: Theme.textSecondary
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: qsTr("安装到 RML 目录：")
                    font.pixelSize: 13
                    color: Theme.textSecondary
                }

                Repeater {
                    model: [8, 17, 21]

                    delegate: Button {
                        id: installButton
                        text: qsTr("Java %1").arg(modelData)
                        enabled: !bridge.busy
                        implicitWidth: 110
                        implicitHeight: 38

                        contentItem: Text {
                            text: installButton.text
                            color: installButton.enabled ? Theme.primary : Theme.textSecondary
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: installButton.hovered ? Theme.primarySoft : "#88FFFFFF"
                            border.width: 1
                            border.color: Theme.primary
                        }

                        onClicked: bridge.installJava(modelData)
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Text {
                Layout.fillWidth: true
                text: bridge.status
                font.pixelSize: 13
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }


        }
    }
}
