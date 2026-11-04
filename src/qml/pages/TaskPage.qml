import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null

    function statusLabel(status) {
        if (status === "queued") return qsTr("排队中")
        if (status === "running") return qsTr("下载中")
        if (status === "paused") return qsTr("已暂停")
        if (status === "completed") return qsTr("已完成")
        if (status === "failed") return qsTr("失败")
        if (status === "cancelled") return qsTr("已取消")
        return status
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: qsTr("下载任务")
                color: Theme.text
                font.pixelSize: 22
                font.bold: true
            }

            Text {
                text: qsTr("%1 个活动任务").arg(bridge.activeTasks)
                color: Theme.textSecondary
                font.pixelSize: 12
            }
        }

        GlassPanel {
            backdrop: page.backdrop
            Layout.fillWidth: true
            Layout.fillHeight: true

            // 用 Flickable + Column 而不是 ListView：任务列表每 500ms 可能刷新模型，
            // ListView 换模型会把滚动位置重置到顶部，Flickable 会保留 contentY。
            Flickable {
                id: taskFlick
                anchors.fill: parent
                anchors.margins: 16
                contentWidth: width
                contentHeight: taskColumn.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Column {
                    id: taskColumn
                    width: taskFlick.width
                    spacing: 8

                    Repeater {
                        id: taskRepeater
                        model: bridge.tasks

                        delegate: Rectangle {
                            width: taskColumn.width
                            height: 112
                            radius: Theme.radius
                            color: Theme.glassFill
                            border.width: 1
                            border.color: Theme.glassBorder

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.title
                                        color: Theme.text
                                        font.pixelSize: 13
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: page.statusLabel(modelData.status)
                                        color: modelData.status === "failed" ? Theme.danger
                                               : (modelData.status === "paused"
                                                  ? Theme.textSecondary : Theme.primary)
                                        font.pixelSize: 11
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.status === "failed" ? modelData.error
                                                                        : modelData.stage
                                    color: modelData.status === "failed" ? Theme.danger
                                                                         : Theme.textSecondary
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        height: 6
                                        radius: 3
                                        color: Theme.glassBorder

                                        Rectangle {
                                            width: parent.width
                                                   * Math.max(0, Math.min(1, modelData.progress))
                                            height: parent.height
                                            radius: 3
                                            color: modelData.status === "failed"
                                                   ? Theme.danger : Theme.primary
                                        }
                                    }

                                    Text {
                                        text: Math.round(Math.max(0, Math.min(1, modelData.progress))
                                                         * 100) + "%"
                                        color: Theme.textSecondary
                                        font.pixelSize: 11
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Item { Layout.fillWidth: true }

                                    SoftButton {
                                        text: qsTr("暂停")
                                        textSize: 12
                                        implicitHeight: 30
                                        visible: modelData.status === "running"
                                        onClicked: bridge.pauseTask(modelData.id)
                                    }

                                    SoftButton {
                                        text: qsTr("继续")
                                        textSize: 12
                                        implicitHeight: 30
                                        primary: true
                                        visible: modelData.status === "paused"
                                        onClicked: bridge.resumeTask(modelData.id)
                                    }

                                    SoftButton {
                                        text: qsTr("重试")
                                        textSize: 12
                                        implicitHeight: 30
                                        primary: true
                                        visible: modelData.status === "failed"
                                                 || modelData.status === "cancelled"
                                        onClicked: bridge.retryTask(modelData.id)
                                    }

                                    SoftButton {
                                        text: qsTr("取消")
                                        textSize: 12
                                        implicitHeight: 30
                                        visible: modelData.status === "running"
                                                 || modelData.status === "paused"
                                                 || modelData.status === "queued"
                                        onClicked: bridge.cancelTask(modelData.id)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: taskRepeater.count === 0
                text: qsTr("目前没有下载任务")
                color: Theme.textSecondary
                font.pixelSize: 13
            }
        }
    }
}
