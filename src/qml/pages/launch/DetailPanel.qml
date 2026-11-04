import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 启动页右栏：版本详情、Java、启动参数与启动按钮。
GlassPanel {
    id: root

    property bool detailsOpen: false

    signal modsRequested()

    function applyOptions() {
        var parsed = parseInt(memoryField.text)
        var valid = !isNaN(parsed) && parsed >= 512 && parsed <= 32768
        memoryWarning.visible = !valid
        if (!valid) return
        bridge.applyLaunchOptions(parsed, gcBox.currentText, argsField.text)
    }

    ColumnLayout {
        id: bodyColumn
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            spacing: 10

            Text {
                text: bridge.selectedName
                font.pixelSize: 20
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
                    color: Theme.primaryText
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        Text {
            Layout.fillWidth: true
            text: bridge.details
            color: Theme.textSecondary
            font.pixelSize: 13
            lineHeight: 1.4
            wrapMode: Text.WrapAnywhere
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.divider
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("Java")
                font.pixelSize: 12
                color: Theme.textSecondary
            }

            SoftComboBox {
                id: javaBox
                Layout.preferredWidth: 300
                model: bridge.javaOptions
                currentIndex: 0
                onActivated: bridge.selectJava(index)
            }

            Text {
                Layout.fillWidth: true
                text: bridge.javaInfo
                font.pixelSize: 12
                color: Theme.textSecondary
                wrapMode: Text.WrapAnywhere
            }
        }
    }

    // 裁剪框：上边贴正文底，下边贴底栏上沿
    Item {
        id: clipBox
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: bodyColumn.bottom
        anchors.bottom: bottomBar.top
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        clip: true

        Item {
            id: detailsLayer
            anchors.left: parent.left
            anchors.right: parent.right
            height: detailsInner.implicitHeight
            // 初始值不依赖布局结果：没算好高度前先放到框外，且不播动画
            y: root.detailsOpen ? 14
                                : (clipBox.height > 0 ? clipBox.height : 5000)

            Behavior on y {
                enabled: clipBox.height > 0
                NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                id: detailsInner
                width: parent.width
                y: 0
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.divider
                }

                Text {
                    text: qsTr("启动详情")
                    font.pixelSize: 13
                    font.bold: true
                    color: Theme.text
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: qsTr("内存 MB")
                        font.pixelSize: 12
                        color: Theme.textSecondary
                    }

                    SoftField {
                        id: memoryField
                        Layout.preferredWidth: 110
                        text: String(bridge.memoryMb)
                        validator: IntValidator { bottom: 512; top: 32768 }
                        onTextEdited: applyTimer.restart()
                    }

                    Text {
                        text: qsTr("GC")
                        font.pixelSize: 12
                        color: Theme.textSecondary
                    }

                    SoftComboBox {
                        id: gcBox
                        Layout.preferredWidth: 130
                        model: ["自动", "G1", "ZGC", "Parallel"]
                        currentIndex: Math.max(0, model.indexOf(bridge.gcMode))
                        onActivated: root.applyOptions()
                    }
                }

                Text {
                    id: memoryWarning
                    Layout.fillWidth: true
                    visible: false
                    text: qsTr("内存需在 512–32768 MB 之间，当前不会保存")
                    color: Theme.danger
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: qsTr("JVM 参数")
                        font.pixelSize: 12
                        color: Theme.textSecondary
                    }

                    SoftField {
                        id: argsField
                        Layout.fillWidth: true
                        text: bridge.jvmArgs
                        placeholderText: qsTr("例如 -XX:+UseZGC -Dfile.encoding=UTF-8")
                        onTextEdited: applyTimer.restart()
                    }
                }
            }
        }
    }

    // B：状态与按钮
    Item {
        id: bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        height: 76

        RowLayout {
            anchors.fill: parent
            spacing: 10

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    spacing: 5

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 6
                        radius: 3
                        color: Theme.border
                        visible: bridge.launchRepairing || bridge.launchFailed

                        Rectangle {
                            width: bridge.launchFailed
                                   ? parent.width
                                   : parent.width * Math.max(0, Math.min(1, bridge.launchProgress))
                            height: parent.height
                            radius: 3
                            color: bridge.launchFailed ? Theme.danger : Theme.primary
                        }
                    }
                }
            }

            Button {
                text: qsTr("模组")
                visible: bridge.loader.length > 0 && bridge.loader !== qsTr("原版")
                enabled: !bridge.busy && bridge.selectedIndex >= 0
                implicitWidth: 84
                implicitHeight: 40

                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? Theme.primary : Theme.textSecondary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.hovered ? Theme.primarySoft : Theme.glassHoverFill
                    border.width: 1
                    border.color: Theme.primary
                }

                onClicked: root.modsRequested()
            }

            Button {
                text: root.detailsOpen ? qsTr("收起") : qsTr("详细信息")
                enabled: !bridge.busy
                implicitWidth: 110
                implicitHeight: 40

                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? Theme.primary : Theme.textSecondary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.hovered ? Theme.primarySoft : Theme.glassHoverFill
                    border.width: 1
                    border.color: Theme.primary
                }

                onClicked: root.detailsOpen = !root.detailsOpen
            }

            Button {
                text: bridge.launchRepairing
                      ? (bridge.launchCancelling ? qsTr("取消中…") : qsTr("取消"))
                      : (bridge.installingVersion.length > 0 ? qsTr("安装中…")
                         : (bridge.busy ? qsTr("运行中") : qsTr("启动")))
                enabled: bridge.launchRepairing
                         ? !bridge.launchCancelling
                         : (!bridge.busy && bridge.installingVersion.length === 0
                            && bridge.selectedIndex >= 0)
                implicitWidth: 130
                implicitHeight: 40

                contentItem: Text {
                    text: parent.text
                    color: Theme.primaryText
                    font.pixelSize: 14
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: bridge.launchRepairing
                           ? Theme.danger
                           : (!parent.enabled ? Theme.disabledFill
                                              : (parent.pressed ? Theme.primaryPressed : Theme.primary))
                }

                onClicked: bridge.launchRepairing ? bridge.cancelLaunchRepair() : bridge.launchGame()
            }
        }
    }

    Timer {
        id: applyTimer
        interval: 300
        onTriggered: root.applyOptions()
    }
}
