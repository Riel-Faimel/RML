import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import ".."
import "../components"
import "download"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null
    property string category: "version"
    property string kind: "mod"
    property int versionKind: 0
    property var loaderChoice: ({})
    property var loaderVersionChoice: ({})

    function loaderFor(id) { return page.loaderChoice[id] || "原版" }
    function loaderVersionFor(id) { return page.loaderVersionChoice[id] || "" }

    function setLoaderVersion(id, version) {
        var copy = {}
        for (var key in page.loaderVersionChoice) copy[key] = page.loaderVersionChoice[key]
        copy[id] = version
        page.loaderVersionChoice = copy
    }

    function chooseLoader(id, name) {
        page.setLoader(id, name)
        page.setLoaderVersion(id, "")
        if (name === "原版") return
        bridge.loadLoaderVersions(id, name)
        loaderSidebar.openFor(name, id, "")
    }

    function installVersion(kind, index, loader, mcVersion) {
        if (loader === "原版") {
            bridge.installGame(kind, index, loader)
            return
        }
        const version = page.loaderVersionFor(mcVersion)
        if (version.length === 0) {
            page.chooseLoader(mcVersion, loader)
            return
        }
        bridge.installGameWithLoader(kind, index, loader, version)
    }

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

    // 默认就拉一次版本清单，不再依赖手动刷新
    Component.onCompleted: bridge.loadGameVersions()

    FileDialog {
        id: importFiles
        title: qsTr("选择 Mod 文件")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Mod 文件 (*.jar)")]
        onAccepted: {
            var list = selectedFiles && selectedFiles.length > 0 ? selectedFiles : [selectedFile]
            importDialog.openFor(list, bridge.selectedMinecraftVersion(), bridge.loader)
        }
    }

    ImportModDialog {
        id: importDialog
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap

        // 左：分类
        CategoryPanel {
            backdrop: page.backdrop
            Layout.preferredWidth: Theme.sidebarWidth
            Layout.maximumWidth: Theme.sidebarWidth
            Layout.fillHeight: true
            category: page.category
            onActivated: function(key, kind) { page.activate(key, kind) }
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

                // 版本分类
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
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
                                color: kindTab.checked ? Theme.primaryText : Theme.primary
                            }

                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: kindTab.checked ? Theme.primary : Theme.glassHoverFill
                                border.width: 1
                                border.color: Theme.primary
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.divider
                    visible: page.category === "version"
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
                        color: Theme.glassHoverFill
                        border.width: 1
                        border.color: Theme.glassBorder

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
                            color: Theme.primaryText
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: parent.enabled ? Theme.primary : Theme.disabledFill
                        }
                        onClicked: bridge.searchContent(searchField.text, page.kind)
                    }

                    SoftButton {
                        text: qsTr("导入 Mod")
                        visible: page.kind === "mod"
                        implicitHeight: 40
                        onClicked: importFiles.open()
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

                    delegate: Loader {
                        id: rowLoader
                        width: ListView.view.width
                        height: item ? item.height : 46
                        sourceComponent: page.category === "version" ? versionRowComponent
                                                                     : contentRowComponent

                        onLoaded: {
                            item.width = rowLoader.width
                            item.index = index
                            if (page.category === "version") {
                                item.versionId = modelData
                                item.loaderName = Qt.binding(function() {
                                    return page.loaderFor(item.versionId)
                                })
                                item.installRequested.connect(function() {
                                    page.installVersion(page.versionKind, index, item.loaderName,
                                                        item.versionId)
                                })
                                item.serverRequested.connect(function() {
                                    bridge.installServer(page.versionKind, index, item.loaderName)
                                })
                                item.loaderRequested.connect(function(loader) {
                                    page.chooseLoader(item.versionId, loader)
                                })
                            } else {
                                item.title = bridge.contentNames[index] || modelData
                                item.iconUrl = bridge.contentIcons[index] || ""
                                item.description = bridge.contentDescriptions[index] || ""
                                item.installRequested.connect(function() {
                                    bridge.installContent(index)
                                })
                            }
                        }
                    }


                    Text {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        text: page.category === "version"
                              ? (bridge.downloadBusy ? qsTr("正在获取版本清单…")
                                                     : qsTr("没有拿到版本清单，检查网络或软件源"))
                              : qsTr("输入关键词搜索")
                        color: Theme.textSecondary
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 6
                    radius: 3
                    color: Theme.border
                    visible: bridge.downloadBusy || bridge.downloadFailed

                    Rectangle {
                        width: bridge.downloadFailed ? parent.width
                                                     : parent.width * Math.max(0, Math.min(1, bridge.downloadProgress))
                        height: parent.height
                        radius: 3
                        color: bridge.downloadFailed ? Theme.danger : Theme.primary
                    }
                }
            }
        }
    }

    Component {
        id: versionRowComponent
        VersionRow {}
    }

    Component {
        id: contentRowComponent
        ContentRow {}
    }

    LoaderSidebar {
        id: loaderSidebar
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 360
        z: 20
        onChosen: function(mcVersion, version) { page.setLoaderVersion(mcVersion, version) }
    }
}
