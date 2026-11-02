import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null
    property string category: "version"
    property string kind: "mod"

    function activate(key, targetKind) {
        page.category = key
        page.kind = targetKind
        if (key === "version") {
            bridge.loadGameVersions()
        } else {
            bridge.searchContent(searchField.text, targetKind)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap

        // 左：分类
        GlassPanel {
            backdrop: page.backdrop
            Layout.preferredWidth: 200
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                Text {
                    text: qsTr("下载")
                    font.pixelSize: 13
                    color: Theme.textSecondary
                }

                Repeater {
                    model: [
                        { key: "version", kind: "", label: qsTr("游戏版本") },
                        { key: "mod", kind: "mod", label: qsTr("Mod") },
                        { key: "shader", kind: "shader", label: qsTr("光影") },
                        { key: "resourcepack", kind: "resourcepack", label: qsTr("材质包") }
                    ]

                    delegate: Rectangle {
                        id: categoryItem
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        radius: Theme.radius
                        color: page.category === modelData.key ? Theme.primarySoft : "#66FFFFFF"
                        border.width: 1
                        border.color: page.category === modelData.key ? Theme.primary : "#55FFFFFF"

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            verticalAlignment: Text.AlignVCenter
                            text: modelData.label
                            font.pixelSize: 14
                            color: page.category === modelData.key ? Theme.primary : Theme.text
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: page.activate(modelData.key, modelData.kind)
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // 右：内容
        GlassPanel {
            backdrop: page.backdrop
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12

                // 版本分类：只要刷新
                RowLayout {
                    Layout.fillWidth: true
                    visible: page.category === "version"

                    Text {
                        text: qsTr("游戏版本")
                        font.pixelSize: 20
                        font.bold: true
                        color: Theme.text
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("刷新")
                        enabled: !bridge.downloadBusy
                        implicitWidth: 100
                        implicitHeight: 36
                        contentItem: Text {
                            text: parent.text
                            color: Theme.primary
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: "#88FFFFFF"
                            border.width: 1
                            border.color: Theme.primary
                        }
                        onClicked: bridge.loadGameVersions()
                    }
                }

                // 其它分类：搜索
                RowLayout {
                    Layout.fillWidth: true
                    visible: page.category !== "version"
                    spacing: 10

                    ComboBox {
                        id: sourceBox
                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 40
                        model: bridge.contentSources
                        onActivated: bridge.selectContentSource(index)

                        palette.text: Theme.text
                        palette.window: "#FFFFFF"
                        palette.base: "#FFFFFF"
                        palette.button: "#FFFFFF"
                        palette.highlight: Theme.primary
                        palette.highlightedText: "#FFFFFF"

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: "#88FFFFFF"
                            border.width: 1
                            border.color: "#55FFFFFF"
                        }

                        contentItem: Text {
                            leftPadding: 12
                            text: sourceBox.displayText
                            color: Theme.text
                            font.pixelSize: 14
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        radius: Theme.radiusSmall
                        color: "#88FFFFFF"
                        border.width: 1
                        border.color: "#55FFFFFF"

                        TextField {
                            id: searchField
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            placeholderText: qsTr("搜索 Mod / 光影 / 材质包")
                            background: null
                            placeholderTextColor: Theme.textSecondary
                            font.pixelSize: 14
                            color: Theme.text
                            onAccepted: bridge.searchContent(text, page.kind)
                        }
                    }

                    Button {
                        text: qsTr("搜索")
                        enabled: !bridge.downloadBusy
                        implicitWidth: 100
                        implicitHeight: 40
                        contentItem: Text {
                            text: parent.text
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: parent.enabled ? Theme.primary : "#99FFFFFF"
                        }
                        onClicked: bridge.searchContent(searchField.text, page.kind)
                    }
                }

                // 列表
                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: page.category === "version" ? bridge.gameVersions : bridge.contentResults

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 46
                        radius: Theme.radius
                        color: "#66FFFFFF"
                        border.width: 1
                        border.color: "#55FFFFFF"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: modelData
                                elide: Text.ElideRight
                                font.pixelSize: 14
                                color: Theme.text
                            }

                            Button {
                                visible: page.category !== "version"
                                text: qsTr("安装")
                                enabled: !bridge.downloadBusy
                                implicitWidth: 76
                                implicitHeight: 30
                                contentItem: Text {
                                    text: parent.text
                                    color: "#FFFFFF"
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: Theme.radiusSmall
                                    color: parent.enabled ? Theme.primary : "#99FFFFFF"
                                }
                                onClicked: bridge.installContent(index)
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        text: page.category === "version" ? qsTr("点「刷新」取版本清单")
                                                          : qsTr("输入关键词搜索")
                        color: Theme.textSecondary
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: bridge.downloadStatus
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
