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
    property int versionKind: 0
    property var loaderChoice: ({})

    function loaderFor(id) { return page.loaderChoice[id] || "原版" }

    function setLoader(id, name) {
        var copy = {}
        for (var key in page.loaderChoice) copy[key] = page.loaderChoice[key]
        copy[id] = name
        page.loaderChoice = copy
    }

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

                    Repeater {
                        model: [qsTr("正式版"), qsTr("测试版"), qsTr("愚人节版")]

                        delegate: Button {
                            id: kindTab
                            text: modelData
                            checkable: true
                            checked: page.versionKind === index
                            implicitHeight: 34
                            implicitWidth: 88
                            padding: 0
                            onClicked: page.versionKind = index

                            contentItem: Text {
                                text: kindTab.text
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                color: kindTab.checked ? "#FFFFFF" : Theme.primary
                            }

                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: kindTab.checked ? Theme.primary : "#88FFFFFF"
                                border.width: 1
                                border.color: Theme.primary
                            }
                        }
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

                    SoftComboBox {
                        id: sourceBox
                        Layout.preferredWidth: 170
                        model: bridge.contentSources
                        onActivated: bridge.selectContentSource(index)
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

                // 进度条
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    visible: bridge.downloadBusy
                    clip: true

                    Rectangle {
                        anchors.fill: parent
                        radius: 3
                        color: "#55FFFFFF"
                    }

                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(1, bridge.downloadProgress))
                        height: parent.height
                        radius: 3
                        color: Theme.primary
                    }
                }

                // 列表
                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: page.category !== "version"
                           ? bridge.contentResults
                           : (page.versionKind === 0 ? bridge.releaseVersions
                                                     : (page.versionKind === 1 ? bridge.snapshotVersions
                                                                               : bridge.aprilVersions))

                    delegate: Rectangle {
                        id: versionRow
                        property string versionId: modelData
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

                            // 加载器：默认原版，点开可切换（菜单可扩展）
                            Button {
                                id: loaderButton
                                visible: page.category === "version"
                                text: page.loaderFor(versionRow.versionId)
                                enabled: !bridge.downloadBusy
                                implicitWidth: 84
                                implicitHeight: 30
                                padding: 0
                                onClicked: loaderMenu.popup()

                                contentItem: Text {
                                    text: loaderButton.text
                                    color: Theme.primary
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                background: Rectangle {
                                    radius: Theme.radiusSmall
                                    color: loaderButton.hovered ? Theme.primarySoft : "#88FFFFFF"
                                    border.width: 1
                                    border.color: Theme.primary
                                }

                                SoftMenu {
                                    id: loaderMenu
                                    y: loaderButton.height
                                    width: 160

                                    Repeater {
                                        model: ["原版", "Fabric", "Forge", "NeoForge"]

                                        delegate: SoftMenuItem {
                                            text: modelData
                                            onTriggered: page.setLoader(versionRow.versionId, modelData)
                                        }
                                    }
                                }
                            }

                            Button {
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
                                onClicked: page.category === "version"
                                               ? bridge.installGame(page.versionKind, index, page.loaderFor(versionRow.versionId))
                                               : bridge.installContent(index)
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
