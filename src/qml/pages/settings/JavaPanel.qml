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
        anchors.margins: 20
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40

            Text {
                text: qsTr("Java 运行时")
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
            id: javaList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 80
            clip: true
            spacing: 6
            model: bridge.javas

            delegate: Rectangle {
                width: ListView.view.width
                height: 42
                radius: Theme.radiusSmall
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
                        verticalAlignment: Text.AlignVCenter
                        text: modelData
                        elide: Text.ElideMiddle
                        font.pixelSize: 13
                        color: Theme.text
                    }

                    SoftButton {
                        text: qsTr("卸载")
                        textSize: 11
                        implicitWidth: 56
                        implicitHeight: 28
                        visible: bridge.canUninstallJava(index)
                        enabled: !bridge.busy
                        onClicked: bridge.uninstallJava(index)
                    }
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
