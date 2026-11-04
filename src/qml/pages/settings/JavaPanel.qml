import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 设置页：Java 运行时列表与安装。
GlassPanel {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

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
            Layout.minimumHeight: 80
            clip: true
            spacing: 6
            model: bridge.javas

            delegate: Rectangle {
                width: ListView.view.width
                height: 38
                radius: Theme.radiusSmall
                color: Theme.glassFill
                border.width: 1
                border.color: Theme.glassBorder

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

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("安装到 RML 目录")
                font.pixelSize: 12
                color: Theme.textSecondary
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: [8, 17, 21]

                    delegate: SoftButton {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Java %1").arg(modelData)
                        enabled: !bridge.busy
                        onClicked: bridge.installJava(modelData)
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: bridge.status
            font.pixelSize: 12
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
    }
}
