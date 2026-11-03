import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import ".."
import "../components"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null
    property var sourceKinds: bridge.sourceKindOptions
    property var sourceKindLabels: {
        var labels = []
        for (var i = 0; i < page.sourceKinds.length; ++i) {
            labels.push(page.sourceKinds[i].label ? String(page.sourceKinds[i].label)
                                                  : String(page.sourceKinds[i].kind))
        }
        return labels
    }

    function kindAt(index) {
        if (index < 0 || index >= page.sourceKinds.length) return ""
        return String(page.sourceKinds[index].kind)
    }

    function addSource() {
        var kind = page.kindAt(addKindBox.currentIndex)
        var name = addNameField.text.trim()
        var url = addUrlField.text.trim()
        if (kind.length === 0 || name.length === 0 || url.length === 0) return
        bridge.addSource(kind, name, url)
    }

    FileDialog {
        id: backgroundDialog
        title: qsTr("选择背景图片")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)")]
        onAccepted: bridge.setBackground(selectedFile)
    }

    SoftDialog {
        id: addSourceDialog
        title: qsTr("添加软件源")
        width: 460

        onOpened: {
            addKindBox.currentIndex = 0
            addNameField.text = ""
            addUrlField.text = ""
            addNameField.forceActiveFocus()
        }

        onAccepted: page.addSource()

        ColumnLayout {
            width: parent.width
            spacing: 10

            Text {
                text: qsTr("类型")
                font.pixelSize: 12
                color: Theme.textSecondary
            }

            SoftComboBox {
                id: addKindBox
                Layout.fillWidth: true
                model: page.sourceKindLabels
            }

            Text {
                text: qsTr("名称")
                font.pixelSize: 12
                color: Theme.textSecondary
            }

            SoftField {
                id: addNameField
                Layout.fillWidth: true
                placeholderText: qsTr("例如 公司镜像")
            }

            Text {
                text: qsTr("地址")
                font.pixelSize: 12
                color: Theme.textSecondary
            }

            SoftField {
                id: addUrlField
                Layout.fillWidth: true
                placeholderText: qsTr("https://")
                onAccepted: addSourceDialog.accept()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap

        // 左：外观 + Java
        ColumnLayout {
            Layout.preferredWidth: 360
            Layout.maximumWidth: 360
            Layout.fillHeight: true
            spacing: Theme.gap

            GlassPanel {
                backdrop: page.backdrop
                Layout.fillWidth: true
                Layout.preferredHeight: 184

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    Text {
                        text: qsTr("外观")
                        font.pixelSize: 20
                        font.bold: true
                        color: Theme.text
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("使用自己的图片作为启动器背景。")
                        font.pixelSize: 12
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: bridge.hasCustomBackground ? qsTr("已自定义") : qsTr("默认背景")
                            font.pixelSize: 12
                            color: bridge.hasCustomBackground ? Theme.primary : Theme.textSecondary
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        SoftButton {
                            text: qsTr("选择图片")
                            primary: true
                            onClicked: backgroundDialog.open()
                        }

                        SoftButton {
                            text: qsTr("恢复默认")
                            enabled: bridge.hasCustomBackground
                            onClicked: bridge.resetBackground()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            GlassPanel {
                backdrop: page.backdrop
                Layout.fillWidth: true
                Layout.fillHeight: true

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
        }

        // 右：软件源
        GlassPanel {
            backdrop: page.backdrop
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("软件源")
                        font.pixelSize: 20
                        font.bold: true
                        color: Theme.text
                    }

                    SoftButton {
                        text: qsTr("恢复默认")
                        onClicked: bridge.resetSources()
                    }

                    SoftButton {
                        text: qsTr("添加")
                        primary: true
                        onClicked: addSourceDialog.open()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("同一类型按列表顺序取第一条，修改名称或地址后失焦即保存。")
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }

                ListView {
                    id: sourceList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 8
                    model: bridge.sourceRecords

                    delegate: Rectangle {
                        id: sourceCard
                        width: ListView.view.width
                        height: 86
                        radius: Theme.radius
                        color: "#66FFFFFF"
                        border.width: 1
                        border.color: "#55FFFFFF"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: modelData.kindLabel
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                }

                                Button {
                                    id: deleteSourceButton
                                    text: qsTr("删除")
                                    implicitWidth: 48
                                    implicitHeight: 24
                                    padding: 0
                                    onClicked: bridge.removeSource(index)

                                    contentItem: Text {
                                        text: deleteSourceButton.text
                                        font.pixelSize: 11
                                        color: Theme.primary
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    background: Rectangle {
                                        radius: Theme.radiusSmall
                                        color: deleteSourceButton.hovered || deleteSourceButton.down
                                                   ? Theme.primarySoft : "transparent"
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                SoftField {
                                    id: sourceNameField
                                    Layout.preferredWidth: 150
                                    Layout.minimumWidth: 90
                                    text: modelData.name
                                    placeholderText: qsTr("名称")
                                    onEditingFinished: bridge.saveSource(index, sourceNameField.text,
                                                                         sourceUrlField.text)
                                }

                                SoftField {
                                    id: sourceUrlField
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: modelData.url
                                    placeholderText: qsTr("https://")
                                    onEditingFinished: bridge.saveSource(index, sourceNameField.text,
                                                                         sourceUrlField.text)
                                }
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: sourceList.count === 0
                        text: qsTr("还没有软件源")
                        color: Theme.textSecondary
                    }
                }
            }
        }
    }
}
