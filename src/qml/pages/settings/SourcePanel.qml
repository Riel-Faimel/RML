import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 设置页：软件源列表与新增。
GlassPanel {
    id: root

    property var sourceKinds: bridge.sourceKindOptions
    property var sourceKindLabels: {
        var labels = []
        for (var i = 0; i < root.sourceKinds.length; ++i) {
            labels.push(root.sourceKinds[i].label ? String(root.sourceKinds[i].label)
                                                  : String(root.sourceKinds[i].kind))
        }
        return labels
    }

    function kindAt(index) {
        if (index < 0 || index >= root.sourceKinds.length) return ""
        return String(root.sourceKinds[index].kind)
    }

    function addSource() {
        var kind = root.kindAt(addKindBox.currentIndex)
        var name = addNameField.text.trim()
        var url = addUrlField.text.trim()
        if (kind.length === 0 || name.length === 0 || url.length === 0) return
        bridge.addSource(kind, name, url)
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

        onAccepted: root.addSource()

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
                model: root.sourceKindLabels
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
                color: Theme.glassFill
                border.width: 1
                border.color: Theme.glassBorder

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
