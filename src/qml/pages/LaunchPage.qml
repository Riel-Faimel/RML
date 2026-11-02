import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null
    property int pendingDelete: -1
    property int pendingRename: -1
    property bool detailsOpen: false

    function applyOptions() {
        var parsed = parseInt(memoryField.text)
        var memory = isNaN(parsed) ? bridge.memoryMb : parsed
        bridge.applyLaunchOptions(memory, gcBox.currentText, argsField.text)
    }

    Dialog {
        id: confirmDelete
        modal: true
        anchors.centerIn: Overlay.overlay
        title: qsTr("删除版本")
        standardButtons: Dialog.Yes | Dialog.Cancel
        width: 360

        Text {
            width: parent.width
            text: qsTr("确定删除「%1」？版本目录会被永久删除。").arg(page.pendingDelete >= 0 ? bridge.versions[page.pendingDelete] : "")
            wrapMode: Text.WrapAnywhere
            color: Theme.text
            font.pixelSize: 13
        }

        onAccepted: bridge.deleteVersionAt(page.pendingDelete)
    }

    Dialog {
        id: renameDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        title: qsTr("重命名版本")
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 360

        TextField {
            id: renameField
            width: parent.width
            placeholderText: qsTr("新名字")
            font.pixelSize: 13
            color: Theme.text
            placeholderTextColor: Theme.textSecondary
            background: Rectangle {
                radius: Theme.radiusSmall
                color: "#88FFFFFF"
                border.width: 1
                border.color: "#55FFFFFF"
            }
        }

        onAccepted: bridge.renameVersionAt(page.pendingRename, renameField.text)
    }

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
                        color: index === bridge.selectedIndex ? Theme.primarySoft : "#66FFFFFF"
                        border.width: 1
                        border.color: index === bridge.selectedIndex ? Theme.primary : "#55FFFFFF"

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
                                color: moreButton.hovered ? "#88FFFFFF" : "transparent"
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
                                onTriggered: {
                                    page.pendingRename = index
                                    renameField.text = modelData
                                    renameDialog.open()
                                }
                            }
                            SoftMenuItem {
                                text: qsTr("删除版本")
                                onTriggered: {
                                    page.pendingDelete = index
                                    confirmDelete.open()
                                }
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

        // 右：单块玻璃；D 在裁剪框内上滑，裁到 B 的上沿
        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            backdrop: page.backdrop

            ColumnLayout {
                id: bodyColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
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
                    wrapMode: Text.WrapAnywhere
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#33808080"
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
                    y: page.detailsOpen ? 14
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
                            color: "#33808080"
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
                                onActivated: page.applyOptions()
                            }
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

                    Text {
                        Layout.fillWidth: true
                        text: bridge.status
                        font.pixelSize: 12
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }

                    Button {
                        text: page.detailsOpen ? qsTr("收起") : qsTr("详细信息")
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
                            color: parent.hovered ? Theme.primarySoft : "#88FFFFFF"
                            border.width: 1
                            border.color: Theme.primary
                        }

                        onClicked: page.detailsOpen = !page.detailsOpen
                    }

                    Button {
                        text: bridge.installingVersion.length > 0 ? qsTr("安装中…")
                              : (bridge.busy ? qsTr("运行中") : qsTr("启动"))
                        enabled: !bridge.busy && bridge.installingVersion.length === 0 && bridge.selectedIndex >= 0
                        implicitWidth: 130
                        implicitHeight: 40

                        contentItem: Text {
                            text: parent.text
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: !parent.enabled ? "#99FFFFFF" : (parent.pressed ? Theme.primaryPressed : Theme.primary)
                        }

                        onClicked: bridge.launchGame()
                    }
                }
            }

            Timer {
                id: applyTimer
                interval: 300
                onTriggered: page.applyOptions()
            }
        }
    }
}
